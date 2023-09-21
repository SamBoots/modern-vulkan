#pragma once
#include "Utils/Logger.h"
#include "BBMemory.h"

#include "Utils/Utils.h"

namespace BB
{
	namespace String_Specs
	{
		constexpr const size_t multipleValue = 8;
		constexpr const size_t standardSize = 8;
	}

	template<typename CharT>
	class Basic_String
	{
	public:
		Basic_String(Allocator a_allocator);
		Basic_String(Allocator a_allocator, size_t a_size);
		Basic_String(Allocator a_allocator, const CharT* a_String);
		Basic_String(Allocator a_allocator, const CharT* a_String, size_t a_size);
		Basic_String(const Basic_String<CharT>& a_String);
		Basic_String(Basic_String<CharT>&& a_String) noexcept;
		~Basic_String();

		Basic_String& operator=(const Basic_String<CharT>& a_rhs);
		Basic_String& operator=(Basic_String<CharT>&& a_rhs) noexcept;
		bool operator==(const Basic_String<CharT>& a_rhs) const;

		void append(const Basic_String<CharT>& a_String);
		void append(const Basic_String<CharT>& a_String, size_t a_SubPos, size_t a_SubLength);
		void append(const CharT* a_String);
		void append(const CharT* a_String, size_t a_size);
		void insert(size_t a_Pos, const Basic_String<CharT>& a_String);
		void insert(size_t a_Pos, const Basic_String<CharT>& a_String, size_t a_SubPos, size_t a_SubLength);
		void insert(size_t a_Pos, const CharT* a_String);
		void insert(size_t a_Pos, const CharT* a_String, size_t a_size);
		void push_back(const CharT a_Char);
		
		void pop_back();

		bool compare(const Basic_String<CharT>& a_String) const;
		bool compare(const Basic_String<CharT>& a_String, size_t a_size) const;
		bool compare(size_t a_Pos, const Basic_String<CharT>& a_String, size_t a_Subpos, size_t a_size) const;
		bool compare(const CharT* a_String) const;
		bool compare(const CharT* a_String, size_t a_size) const;
		bool compare(size_t a_Pos, const CharT* a_String) const;
		bool compare(size_t a_Pos, const CharT* a_String, size_t a_size) const;

		void clear();

		void reserve(const size_t a_size);
		void shrink_to_fit();

		size_t size() const { return m_size; }
		size_t capacity() const { return m_capacity; }
		CharT* data() const { return m_String; }
		const CharT* c_str() const { return m_String; }

	private:
		void grow(size_t a_MinCapacity = 1);
		void reallocate(size_t a_new_capacity);

		Allocator m_allocator;

		CharT* m_String;
		size_t m_size = 0;
		size_t m_capacity = 64;
	};

	using String = Basic_String<char>;
	using WString = Basic_String<wchar_t>;


	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Allocator a_allocator)
		: Basic_String(a_allocator, String_Specs::standardSize)
	{}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Allocator a_allocator, size_t a_size)
	{
		constexpr bool is_char = std::is_same_v<CharT, char> || std::is_same_v<CharT, wchar_t>;
		BB_STATIC_ASSERT(is_char, "String is not a char or wchar");

		m_allocator = a_allocator;
		m_capacity = RoundUp(a_size, String_Specs::multipleValue);

		m_String = reinterpret_cast<CharT*>(BBalloc(m_allocator, m_capacity * sizeof(CharT)));
		Memory::Set(m_String, NULL, m_capacity);
	}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Allocator a_allocator, const CharT* a_String)
		:	Basic_String(a_allocator, a_String, Memory::StrLength(a_String))
	{}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Allocator a_allocator, const CharT* a_String, size_t a_size)
	{
		constexpr bool is_char = std::is_same_v<CharT, char> || std::is_same_v<CharT, wchar_t>;
		BB_STATIC_ASSERT(is_char, "String is not a char or wchar");

		m_allocator = a_allocator;
		m_capacity = RoundUp(a_size + 1, String_Specs::multipleValue);
		m_size = a_size;

		m_String = reinterpret_cast<CharT*>(BBalloc(m_allocator, m_capacity * sizeof(CharT)));
		Memory::Copy(m_String, a_String, a_size);
		Memory::Set(m_String + a_size, NULL, m_capacity - a_size);
	}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(const Basic_String<CharT>& a_String)
	{
		m_allocator = a_String.m_allocator;
		m_capacity = a_String.m_capacity;
		m_size = a_String.m_size;

		m_String = reinterpret_cast<CharT*>(BBalloc(m_allocator, m_capacity * sizeof(CharT)));
		Memory::Copy(m_String, a_String.m_String, m_capacity);
	}

	template<typename CharT>
	inline BB::Basic_String<CharT>::Basic_String(Basic_String<CharT>&& a_String) noexcept
	{
		m_allocator = a_String.m_allocator;
		m_capacity = a_String.m_capacity;
		m_size = a_String.m_size;
		m_String = a_String.m_String;

		a_String.m_allocator.allocator = nullptr;
		a_String.m_allocator.func = nullptr;
		a_String.m_capacity = 0;
		a_String.m_size = 0;
		a_String.m_String = nullptr;
	}

	template<typename CharT>
	inline BB::Basic_String<CharT>::~Basic_String()
	{
		if (m_String != nullptr)
		{
			BBfree(m_allocator, m_String);
			m_String = nullptr;
		}
	}

	template<typename CharT>
	inline Basic_String<CharT>& BB::Basic_String<CharT>::operator=(const Basic_String<CharT>& a_rhs)
	{
		this->~Basic_String();

		m_allocator = a_rhs.m_allocator;
		m_capacity = a_rhs.m_capacity;
		m_size = a_rhs.m_size;

		m_String = reinterpret_cast<CharT*>(BBalloc(m_allocator, m_capacity * sizeof(CharT)));
		Memory::Copy(m_String, a_rhs.m_String, m_capacity);

		return *this;
	}

	template<typename CharT>
	inline Basic_String<CharT>& BB::Basic_String<CharT>::operator=(Basic_String<CharT>&& a_rhs) noexcept
	{
		this->~Basic_String();

		m_allocator = a_rhs.m_allocator;
		m_capacity = a_rhs.m_capacity;
		m_size = a_rhs.m_size;
		m_String = a_rhs.m_String;

		a_rhs.m_allocator.allocator = nullptr;
		a_rhs.m_allocator.func = nullptr;
		a_rhs.m_capacity = 0;
		a_rhs.m_size = 0;
		a_rhs.m_String = nullptr;

		return *this;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::operator==(const Basic_String<CharT>& a_rhs) const
	{
		if (Memory::Compare(m_String, a_rhs.data(), m_size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::append(const Basic_String<CharT>& a_String)
	{
		append(a_String.c_str(), a_String.size());
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::append(const Basic_String<CharT>& a_String, size_t a_SubPos, size_t a_SubLength)
	{
		append(a_String.c_str() + a_SubPos, a_SubLength);
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::append(const CharT* a_String)
	{
		append(a_String, Memory::StrLength(a_String));
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::append(const CharT* a_String, size_t a_size)
	{
		if (m_size + 1 + a_size >= m_capacity)
			grow(a_size + 1);

		BB::Memory::Copy(m_String + m_size, a_String, a_size);
		m_size += a_size;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::insert(size_t a_Pos, const Basic_String<CharT>& a_String)
	{
		insert(a_Pos, a_String.c_str(), a_String.size());
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::insert(size_t a_Pos, const Basic_String<CharT>& a_String, size_t a_SubPos, size_t a_SubLength)
	{
		insert(a_Pos, a_String.c_str() + a_SubPos, a_SubLength);
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::insert(size_t a_Pos, const CharT* a_String)
	{
		insert(a_Pos, a_String, Memory::StrLength(a_String));
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::insert(size_t a_Pos, const CharT* a_String, size_t a_size)
	{
		BB_ASSERT(m_size >= a_Pos, "String::Insert, trying to insert a string in a invalid position.");

		if (m_size + 1 + a_size >= m_capacity)
			grow(a_size + 1);

		Memory::sMove(m_String + (a_Pos + a_size), m_String + a_Pos, m_size - a_Pos);

		Memory::Copy(m_String + a_Pos, a_String, a_size);
		m_size += a_size;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::push_back(const CharT a_Char)
	{
		if (m_size + 1 >= m_capacity)
			grow();

		m_String[m_size++] = a_Char;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::pop_back()
	{
		m_String[m_size--] = NULL;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(const Basic_String<CharT>& a_String) const
	{
		if (Memory::Compare(m_String, a_String.data(), m_size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(const Basic_String<CharT>& a_String, size_t a_size) const
	{
		if (Memory::Compare(m_String, a_String.c_str(), a_size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(size_t a_Pos, const Basic_String<CharT>& a_String, size_t a_Subpos, size_t a_size) const
	{
		if (Memory::Compare(m_String + a_Pos, a_String.c_str() + a_Subpos, a_size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(const CharT* a_String) const
	{
		return compare(a_String, Memory::StrLength(a_String));
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(const CharT* a_String, size_t a_size) const
	{
		if (Memory::Compare(m_String, a_String, a_size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(size_t a_Pos, const CharT* a_String) const
	{
		return compare(a_Pos, a_String, Memory::StrLength(a_String));
	}

	template<typename CharT>
	inline bool BB::Basic_String<CharT>::compare(size_t a_Pos, const CharT* a_String, size_t a_size) const
	{
		if (Memory::Compare(m_String + a_Pos, a_String, a_size) == 0)
			return true;
		return false;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::clear()
	{
		Memory::Set(m_String, NULL, m_size);
		m_size = 0;
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::reserve(const size_t a_size)
	{
		if (a_size > m_capacity)
		{
			size_t t_ModifiedCapacity = RoundUp(a_size + 1, String_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
		}
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::shrink_to_fit()
	{
		size_t t_ModifiedCapacity = RoundUp(m_size + 1, String_Specs::multipleValue);
		if (t_ModifiedCapacity < m_capacity)
		{
			reallocate(t_ModifiedCapacity);
		}
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::grow(size_t a_MinCapacity)
	{
		size_t t_ModifiedCapacity = m_capacity * 2;

		if (a_MinCapacity > t_ModifiedCapacity)
			t_ModifiedCapacity = RoundUp(a_MinCapacity, String_Specs::multipleValue);

		reallocate(t_ModifiedCapacity);
	}

	template<typename CharT>
	inline void BB::Basic_String<CharT>::reallocate(size_t a_new_capacity)
	{
		CharT* t_NewString = reinterpret_cast<CharT*>(BBalloc(m_allocator, a_new_capacity * sizeof(CharT)));

		Memory::Copy(t_NewString, m_String, m_size);
		BBfree(m_allocator, m_String);

		m_String = t_NewString;
		m_capacity = a_new_capacity;
	}


	template<typename CharT, size_t stringSize>
	class Stack_String
	{
	public:
		Stack_String()
		{
			Memory::Set(m_String, 0, stringSize);
		};
		Stack_String(const CharT* a_String) : Stack_String(a_String, Memory::StrLength(a_String)) {};
		Stack_String(const CharT* a_String, size_t a_size)
		{
			BB_ASSERT(a_size < sizeof(m_String), "Stack string overflow");
			Memory::Set(m_String, 0, sizeof(m_String));
			Memory::Copy(m_String, a_String, a_size);
			m_size = a_size;
		};
		Stack_String(const Stack_String<CharT, stringSize>& a_String)
		{
			Memory::Copy(m_String, a_String, sizeof(m_String));
			m_size = a_String.size;
		};
		Stack_String(Stack_String<CharT, stringSize>&& a_String) noexcept
		{
			Memory::Copy(m_String, a_String, sizeof(m_String));
			m_size = a_String.size();

			Memory::Set(a_String.m_String, 0, stringSize);
			a_String.m_size = 0
		};
		~Stack_String()
		{
			clear();
		};

		Stack_String& operator=(const Stack_String<CharT, stringSize>& a_rhs)
		{
			this->~Stack_String();

			Memory::Copy(m_String, a_String, sizeof(m_String));
			m_size = a_String.size();
		};
		Stack_String& operator=(Stack_String<CharT, stringSize>&& a_rhs) noexcept
		{
			this->~Stack_String();

			Memory::Copy(m_String, a_String, sizeof(m_String));
			m_size = a_String.size();

			Memory::Set(a_rhs.m_String, 0, stringSize);
			a_rhs.m_size = 0
		};
		bool operator==(const Stack_String<CharT, stringSize>& a_rhs) const
		{
			if (Memory::Compare(m_String, a_rhs.data(), sizeof(m_String)) == 0)
				return true;
			return false;
		};

		void append(const Stack_String<CharT, stringSize>& a_String)
		{
			append(a_String.c_str(), a_String.size());
		};
		void append(const Stack_String<CharT, stringSize>& a_String, size_t a_SubPos, size_t a_SubLength)
		{
			append(a_String.c_str() + a_SubPos, a_SubLength);
		};
		void append(const CharT* a_String)
		{
			append(a_String, Memory::StrLength(a_String));
		};
		void append(const CharT a_char, size_t a_count = 1)
		{
			BB_ASSERT(m_size + a_count < sizeof(m_String), "Stack string overflow");
			for (size_t i = 0; i < a_count; i++)
				m_String[m_size++] = a_char;
		};
		void append(const CharT* a_String, size_t a_size)
		{
			BB_ASSERT(m_size + a_size < sizeof(m_String), "Stack string overflow");
			BB::Memory::Copy(m_String + m_size, a_String, a_size);
			m_size += a_size;
		};
		void insert(size_t a_Pos, const Stack_String<CharT, stringSize>& a_String)
		{
			insert(a_Pos, a_String.c_str(), a_String.size());
		};
		void insert(size_t a_Pos, const Stack_String<CharT, stringSize>& a_String, size_t a_SubPos, size_t a_SubLength)
		{
			insert(a_Pos, a_String.c_str() + a_SubPos, a_SubLength);
		}
		void insert(size_t a_Pos, const CharT* a_String)
		{
			insert(a_Pos, a_String, Memory::StrLength(a_String));
		};
		void insert(size_t a_Pos, const CharT* a_String, size_t a_size)
		{
			BB_ASSERT(m_size >= a_Pos, "Trying to insert a string in a invalid position.");
			BB_ASSERT(m_size + a_size < sizeof(m_String), "Stack string overflow");

			Memory::sMove(m_String + (a_Pos + a_size), m_String + a_Pos, m_size - a_Pos);

			Memory::Copy(m_String + a_Pos, a_String, a_size);
			m_size += a_size;
		};
		void push_back(const CharT a_Char)
		{
			m_String[m_size++] = a_Char;
			BB_ASSERT(m_size < sizeof(m_String), "Stack string overflow");
		};

		void pop_back(uint32_t a_Count)
		{
			m_size -= a_Count;
			memset(Pointer::Add(m_String, m_size), NULL, a_Count);
		};

		void clear()
		{
			Memory::Set(m_String, 0, stringSize);
			m_size = 0;
		}

		size_t size() const { return m_size; }
		size_t capacity() const { return stringSize; }
		CharT* data() { return m_String; }
		const CharT* c_str() const { return m_String; }

	private:
		CharT m_String[stringSize + 1];
		size_t m_size = 0;
	};

	template<size_t stringSize>
	using StackString = Stack_String<char, stringSize>;
	template<size_t stringSize>
	using StackWString = Stack_String<wchar_t, stringSize>;
}