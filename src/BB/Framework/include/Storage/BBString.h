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

			m_string = reinterpret_cast<CharT*>(ArenaAlloArrc(a_arena, CharT, m_capacity));
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

		void reserve(MemoryArena& a_arena, const size_t a_size)
		{
			if (a_size > m_capacity)
			{
				size_t modified_capacity = RoundUp(a_size + 1, String_Specs::multiple_value);

				reallocate(a_arena, modified_capacity);
			}
		}

		size_t size() const { return m_size; }
		size_t capacity() const { return m_capacity; }
		CharT* data() const { return m_string; }
		const CharT* c_str() const { return m_string; }

	private:
		void reallocate(MemoryArena& a_arena, size_t a_new_capacity)
		{
			//do a realloc maybe?
			CharT* new_string = reinterpret_cast<CharT*>(ArenaAlloArrc(a_arena, CharT, a_new_capacity));

			Memory::Copy(new_string, m_string, m_size);

			m_string = new_string;
			m_capacity = a_new_capacity;
		}

		CharT* m_string;
		size_t m_size;
		size_t m_capacity;
	};

	using String = Basic_String<char>;
	using WString = Basic_String<wchar_t>;

	template<typename CharT, size_t stringSize>
	class Stack_String
	{
	public:
		Stack_String()
		{
			Memory::Set(m_string, 0, stringSize);
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
		Stack_String(const Stack_String<CharT, stringSize>& a_string)
		{
			Memory::Copy(m_string, a_string, sizeof(m_string));
			m_size = a_string.size;
		}
		Stack_String(Stack_String<CharT, stringSize>&& a_string) noexcept
		{
			Memory::Copy(m_string, a_string, sizeof(m_string));
			m_size = a_string.size();

			Memory::Set(a_string.m_string, 0, stringSize);
			a_string.m_size = 0
		}
		~Stack_String()
		{
			clear();
		}

		Stack_String& operator=(const Stack_String<CharT, stringSize>& a_rhs)
		{
			this->~Stack_String();

			Memory::Copy(m_string, a_string, sizeof(m_string));
			m_size = a_string.size();
		}
		Stack_String& operator=(Stack_String<CharT, stringSize>&& a_rhs) noexcept
		{
			this->~Stack_String();

			Memory::Copy(m_string, a_string, sizeof(m_string));
			m_size = a_string.size();

			Memory::Set(a_rhs.m_string, 0, stringSize);
			a_rhs.m_size = 0
		}
		bool operator==(const Stack_String<CharT, stringSize>& a_rhs) const
		{
			if (Memory::Compare(m_string, a_rhs.data(), sizeof(m_string)) == 0)
				return true;
			return false;
		}

		void append(const Stack_String<CharT, stringSize>& a_string)
		{
			append(a_string.c_str(), a_string.size());
		}
		void append(const Stack_String<CharT, stringSize>& a_string, size_t a_sub_pos, size_t a_sub_length)
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
		void insert(size_t a_pos, const Stack_String<CharT, stringSize>& a_string)
		{
			insert(a_pos, a_string.c_str(), a_string.size());
		}
		void insert(size_t a_pos, const Stack_String<CharT, stringSize>& a_string, size_t a_sub_pos, size_t a_sub_length)
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
		void push_back(const CharT a_Char)
		{
			m_string[m_size++] = a_Char;
			BB_ASSERT(m_size < sizeof(m_string), "Stack string overflow");
		}

		void pop_back(uint32_t a_count)
		{
			m_size -= a_count;
			memset(Pointer::Add(m_string, m_size), NULL, a_count);
		}

		void clear()
		{
			Memory::Set(m_string, 0, stringSize);
			m_size = 0;
		}

		size_t size() const { return m_size; }
		size_t capacity() const { return stringSize; }
		CharT* data() { return m_string; }
		const CharT* c_str() const { return m_string; }

	private:
		CharT m_string[stringSize + 1];
		size_t m_size = 0;
	};

	template<size_t string_size>
	using StackString = Stack_String<char, string_size>;
	template<size_t string_size>
	using StackWString = Stack_String<wchar_t, string_size>;
}
