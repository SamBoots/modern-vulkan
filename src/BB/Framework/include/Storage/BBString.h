#pragma once
#include "Utils/Logger.h"
#include "MemoryArena.hpp"

#include "Utils/Utils.h"

namespace BB
{
	namespace String_Specs
	{
		constexpr const size_t multiple_value = 8;
		constexpr const size_t standard_size = 8;
	}

	template<typename CharT>
	class String_View
	{
	public:
		String_View(const CharT* a_string) : String_View(a_string, Memory::StrLength(a_string)) {}
		String_View(const CharT* a_string, const size_t a_size) : m_string_view(a_string), m_size(a_size) {}

		bool operator==(const String_View<CharT>& a_rhs) const
		{
			return Compare(a_rhs.c_str(), a_rhs.size);
		}

		size_t find_first_of(const CharT a_char) const
		{
			for (size_t i = 0; i < m_size; i++)
			{
				if (m_string_view[i] == a_char)
					return i;
			}
			return size_t(-1);
		}

		size_t find_last_of(const CharT a_char) const
		{
			size_t last_pos = size_t(-1);

			for (size_t i = 0; i < m_size; i++)
			{
				if (m_string_view[i] == a_char)
					last_pos = i;
			}

			return last_pos;
		}

		size_t find_last_of_directory_slash() const
		{
			size_t last_pos = size_t(-1);

			for (size_t i = 0; i < m_size; i++)
			{
				if (m_string_view[i] == '\\' || m_string_view[i] == '/')
					last_pos = i;
			}

			return last_pos;
		}

		bool compare(const size_t a_pos, const CharT* a_str) const
		{
			return compare(a_pos, a_str, Memory::StrLength(a_str) - 1);
		}

		bool compare(const size_t a_pos, const CharT* a_str, const size_t a_str_size) const
		{
			BB_ASSERT(a_pos + a_str_size <= m_size, "trying to read the string out of bounds");

			// maybe optimize that big strings are early out'd and see if memcmp works better

			for (size_t i = 0; i < a_str_size; i++)
			{
				if (m_string_view[a_pos + i] != a_str[i])
					return false;
			}
			return true;
		}

		size_t size() const { return m_size; }
		const CharT* data() const { return m_string_view; }
		const CharT* c_str() const { return m_string_view; }

	private:
		const CharT* m_string_view;
		const size_t m_size;
	};

	using StringView = String_View<char>;
	using StringWView = String_View<wchar_t>;

	template<typename CharT>
	class Basic_String
	{
	public:
		Basic_String() = default;
		Basic_String(MemoryArena& a_arena)
			: Basic_String(a_arena, String_Specs::standard_size)
		{}
		Basic_String(MemoryArena& a_arena, size_t a_size)
		{
			constexpr bool is_char = std::is_same_v<CharT, char> || std::is_same_v<CharT, wchar_t>;
			BB_STATIC_ASSERT(is_char, "String is not a char or wchar");

			m_capacity = RoundUp(a_size, String_Specs::multiple_value);
			m_size = 0;

			m_string = reinterpret_cast<CharT*>(ArenaAllocArr(a_arena, CharT, m_capacity));
			Memory::Set(m_string, NULL, m_capacity);
		}
		Basic_String(MemoryArena& a_arena, const CharT* const a_string)
			: Basic_String(a_arena, a_string, Memory::StrLength(a_string))
		{}
		Basic_String(MemoryArena& a_arena, const CharT* const a_string, size_t a_size)
		{
			constexpr bool is_char = std::is_same_v<CharT, char> || std::is_same_v<CharT, wchar_t>;
			BB_STATIC_ASSERT(is_char, "String is not a char or wchar");

			m_capacity = RoundUp(a_size + 1, String_Specs::multiple_value);
			m_size = a_size;

			m_string = reinterpret_cast<CharT*>(ArenaAllocArr(a_arena, CharT, m_capacity));
			Memory::Copy(m_string, a_string, a_size);
			Memory::Set(m_string + a_size, NULL, m_capacity - a_size);
		}
		Basic_String(const Basic_String<CharT>& a_string) = delete;
		Basic_String(Basic_String<CharT>&& a_string) noexcept
		{
			m_capacity = a_string.m_capacity;
			m_size = a_string.m_size;
			m_string = a_string.m_string;

			a_string.m_capacity = 0;
			a_string.m_size = 0;
			a_string.m_string = nullptr;
		}

		Basic_String& operator=(const Basic_String<CharT>& a_rhs) = delete;
		Basic_String& operator=(Basic_String<CharT>&& a_rhs) noexcept
		{
			this->~Basic_String();

			m_capacity = a_rhs.m_capacity;
			m_size = a_rhs.m_size;
			m_string = a_rhs.m_string;

			a_rhs.m_capacity = 0;
			a_rhs.m_size = 0;
			a_rhs.m_string = nullptr;

			return *this;
		}
		bool operator==(const Basic_String<CharT>& a_rhs) const
		{
			if (Memory::Compare(m_string, a_rhs.data(), m_size) == 0)
				return true;
			return false;
		}

		bool append(const Basic_String<CharT>& a_string)
		{
			return append(a_string.c_str(), a_string.size());
		}
		bool append(const Basic_String<CharT>& a_string, size_t a_sub_pos, size_t a_sub_length)
		{
			return append(a_string.c_str() + a_sub_pos, a_sub_length);
		}
		bool append(const CharT* const a_string)
		{
			return append(a_string, Memory::StrLength(a_string));
		}
		bool append(const CharT* const a_string, size_t a_size)
		{
			if (m_size + 1 + a_size >= m_capacity)
				return false;

			BB::Memory::Copy(m_string + m_size, a_string, a_size);
			m_size += a_size;
			return true;
		}
		bool insert(size_t a_pos, const Basic_String<CharT>& a_string)
		{
			return insert(a_pos, a_string.c_str(), a_string.size());
		}
		bool insert(size_t a_pos, const Basic_String<CharT>& a_string, size_t a_sub_pos, size_t a_sub_length)
		{
			return insert(a_pos, a_string.c_str() + a_sub_pos, a_sub_length);
		}
		bool insert(size_t a_pos, const CharT* a_string)
		{
			return insert(a_pos, a_string, Memory::StrLength(a_string));
		}
		bool insert(size_t a_pos, const CharT* a_string, size_t a_size)
		{
			BB_ASSERT(m_size >= a_pos, "String::Insert, trying to insert a string in a invalid position.");

			if (m_size + 1 + a_size >= m_capacity)
				return false;

			Memory::Move(m_string + (a_pos + a_size), m_string + a_pos, m_size - a_pos);

			Memory::Copy(m_string + a_pos, a_string, a_size);
			m_size += a_size;
			return true;
		}
		bool push_back(const CharT a_Char)
		{
			if (m_size + 1 >= m_capacity)
				return false;

			m_string[m_size++] = a_Char;
			return true;
		}

		bool push_directory_slash()
		{
#ifdef _WIN32
			push_back('\\');
#elif _UNIX
			push_back('/');
			return tre;
#else
			BB_STATIC_ASSERT(false, "no OS define given for push_directory_slash");
			return false;
#endif // OS name
		}
		
		void pop_back()
		{
			m_string[m_size--] = NULL;
		}

		bool compare(const Basic_String<CharT>& a_string) const
		{
			if (Memory::Compare(m_string, a_string.data(), m_size) == 0)
				return true;
			return false;
		}
		bool compare(const Basic_String<CharT>& a_string, size_t a_size) const
		{
			if (Memory::Compare(m_string, a_string.c_str(), a_size) == 0)
				return true;
			return false;
		}
		bool compare(size_t a_pos, const Basic_String<CharT>& a_string, size_t a_sub_pos, size_t a_size) const
		{
			if (Memory::Compare(m_string + a_pos, a_string.c_str() + a_sub_pos, a_size) == 0)
				return true;
			return false;
		}
		bool compare(const CharT* a_string) const
		{
			return compare(a_string, Memory::StrLength(a_string));
		}
		bool compare(const CharT* a_string, size_t a_size) const
		{
			if (Memory::Compare(m_string, a_string, a_size) == 0)
				return true;
			return false;
		}
		bool compare(size_t a_pos, const CharT* a_string) const
		{
			return compare(a_pos, a_string, Memory::StrLength(a_string));
		}
		bool compare(size_t a_pos, const CharT* a_string, size_t a_size) const
		{
			if (Memory::Compare(m_string + a_pos, a_string, a_size) == 0)
				return true;
			return false;
		}

		void clear()
		{
			Memory::Set(m_string, NULL, m_size);
			m_size = 0;
		}

		size_t size() const { return m_size; }
		size_t capacity() const { return m_capacity; }
		CharT* data() const { return m_string; }
		const CharT* c_str() const { return m_string; }

	private:
		CharT* m_string;
		size_t m_size;
		size_t m_capacity;
	};

	using String = Basic_String<char>;
	using WString = Basic_String<wchar_t>;

	template<typename CharT, size_t STRING_SIZE>
	class Stack_String
	{
	public:
		Stack_String()
		{
			Memory::Set(m_string, 0, STRING_SIZE);
		}
		Stack_String(const CharT* a_string) 
			: Stack_String(a_string, Memory::StrLength(a_string)) {}
		Stack_String(const CharT* a_string, size_t a_size)
		{
			BB_ASSERT(a_size < sizeof(m_string), "Stack string overflow");
			Memory::Set(m_string, 0, sizeof(m_string));
			Memory::Copy(m_string, a_string, a_size);
			m_size = a_size;
		}
		Stack_String(const Stack_String<CharT, STRING_SIZE>& a_string)
		{
			Memory::Copy(m_string, a_string.m_string, sizeof(m_string));
			m_size = a_string.size;
		}
		Stack_String(Stack_String<CharT, STRING_SIZE>&& a_string) noexcept
		{
			Memory::Copy(m_string, a_string.m_string, sizeof(m_string));
			m_size = a_string.size();

			Memory::Set(a_string.m_string, 0, STRING_SIZE);
			a_string.m_size = 0;
		}
		~Stack_String()
		{
			clear();
		}

		Stack_String& operator=(const Stack_String<CharT, STRING_SIZE>& a_rhs)
		{
			this->~Stack_String();

			Memory::Copy(m_string, a_string, sizeof(m_string));
			m_size = a_string.size();
		}
		Stack_String& operator=(Stack_String<CharT, STRING_SIZE>&& a_rhs) noexcept
		{
			this->~Stack_String();

			Memory::Copy(m_string, a_string, sizeof(m_string));
			m_size = a_string.size();

			Memory::Set(a_rhs.m_string, 0, STRING_SIZE);
			a_rhs.m_size = 0
		}
		bool operator==(const Stack_String<CharT, STRING_SIZE>& a_rhs) const
		{
			if (Memory::Compare(m_string, a_rhs.data(), sizeof(m_string)) == 0)
				return true;
			return false;
		}

		template<size_t PARAM_STRING_SIZE>
		void append(const Stack_String<CharT, PARAM_STRING_SIZE>& a_string)
		{
			append(a_string.c_str(), a_string.size());
		}
		void append(const Stack_String<CharT, STRING_SIZE>& a_string, size_t a_sub_pos, size_t a_sub_length)
		{
			append(a_string.c_str() + a_sub_pos, a_sub_length);
		}
		void append(const CharT* a_string)
		{
			append(a_string, Memory::StrLength(a_string));
		}
		void append(const CharT a_char, size_t a_count = 1)
		{
			BB_ASSERT(m_size + a_count < sizeof(m_string), "Stack string overflow");
			for (size_t i = 0; i < a_count; i++)
				m_string[m_size++] = a_char;
		}
		void append(const CharT* a_string, size_t a_size)
		{
			BB_ASSERT(m_size + a_size < sizeof(m_string), "Stack string overflow");
			BB::Memory::Copy(m_string + m_size, a_string, a_size);
			m_size += a_size;
		}
		void insert(size_t a_pos, const Stack_String<CharT, STRING_SIZE>& a_string)
		{
			insert(a_pos, a_string.c_str(), a_string.size());
		}
		void insert(size_t a_pos, const Stack_String<CharT, STRING_SIZE>& a_string, size_t a_sub_pos, size_t a_sub_length)
		{
			insert(a_pos, a_string.c_str() + a_sub_pos, a_sub_length);
		}
		void insert(size_t a_pos, const CharT* a_string)
		{
			insert(a_pos, a_string, Memory::StrLength(a_string));
		}
		void insert(size_t a_pos, const CharT* a_string, size_t a_size)
		{
			BB_ASSERT(m_size >= a_pos, "Trying to insert a string in a invalid position.");
			BB_ASSERT(m_size + a_size < sizeof(m_string), "Stack string overflow");

			Memory::sMove(m_string + (a_pos + a_size), m_string + a_pos, m_size - a_pos);

			Memory::Copy(m_string + a_pos, a_string, a_size);
			m_size += a_size;
		}
		void push_back(const CharT a_char)
		{
			m_string[m_size++] = a_char;
			BB_ASSERT(m_size < sizeof(m_string), "Stack string overflow");
		}

		void pop_back(uint32_t a_count)
		{
			m_size -= a_count;
			memset(Pointer::Add(m_string, m_size), NULL, a_count);
		}

		bool push_directory_slash()
		{
#ifdef _WIN32
			return push_back('\\');
#elif _UNIX
			return push_back('/');
#else
			BB_STATIC_ASSERT(false, "no OS define given for push_directory_slash");
			return false;
#endif // OS name
		}

		// with ::Data() you can modify the string without touching the class such as interacting with some C api's like the windows API. 
		// this function will recalculate how big the string is.
		void RecalculateStringSize()
		{
			m_size = strnlen_s(m_string, STRING_SIZE);
		}

		size_t find_first_of(const CharT a_char) const
		{
			for (size_t i = 0; i < m_size; i++)
			{
				if (m_string[i] == a_char)
					return i;
			}
			return size_t(-1);
		}

		size_t find_last_of(const CharT a_char) const
		{
			size_t last_pos = size_t(-1);

			for (size_t i = 0; i < m_size; i++)
			{
				if (m_string[i] == a_char)
					last_pos = i;
			}

			return last_pos;
		}

		bool compare(const size_t a_pos, const CharT* a_str) const
		{
			return compare(a_pos, a_str, Memory::StrLength(a_str));
		}

		bool compare(const size_t a_pos, const CharT* a_str, const size_t a_str_size) const
		{
			if (a_pos + a_str_size > m_size)
				return false;

			for (size_t i = 0; i < a_str_size; i++)
			{
				if (m_string[a_pos + i] != a_str[i])
					return false;
			}
			return true;
		}

		void clear()
		{
			Memory::Set(m_string, 0, STRING_SIZE);
			m_size = 0;
		}

		size_t size() const { return m_size; }
		constexpr size_t capacity() const { return STRING_SIZE; }
		CharT* data() { return m_string; }
		const CharT* c_str() const { return m_string; }

	private:
		CharT m_string[STRING_SIZE + 1];
		size_t m_size = 0;
	};

	template<size_t STRING_SIZE>
	using StackString = Stack_String<char, STRING_SIZE>;
	template<size_t STRING_SIZE>
	using StackWString = Stack_String<wchar_t, STRING_SIZE>;
}
