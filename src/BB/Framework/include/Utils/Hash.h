#pragma once
#include "Utils/Logger.h"
#include <type_traits>

//will remove this, I don't like it.
//Maybe a unified hash is cringe and I should just have some basic hashing operations in this file.
struct Hash
{
	Hash() {}
	Hash(uint64_t a_Hash) : hash(a_Hash) {}
	uint64_t hash = 0;

	operator const uint64_t() const { return hash; }
	void operator=(const uint64_t a_rhs) { hash = a_rhs; }
	Hash operator++(int) { return hash++; }
	void operator*=(size_t a_Multi) { hash *= a_Multi; }

	//Create with uint64_t.
	static Hash MakeHash(size_t a_value);
	static Hash MakeHash(const char* a_value);
	static Hash MakeHash(void* a_value);

private:

};

inline Hash Hash::MakeHash(size_t a_value)
{
	a_value ^= a_value << 13;
	a_value ^= a_value >> 17;
	a_value ^= a_value << 5;
	return Hash(a_value);
}

inline Hash Hash::MakeHash(void* a_value)
{
	uintptr_t t_Value = reinterpret_cast<uintptr_t>(a_value);

	t_Value ^= t_Value << 13;
	t_Value ^= t_Value >> 17;
	t_Value ^= t_Value << 5;
	return Hash(t_Value);
}

inline Hash Hash::MakeHash(const char* a_value)
{
	uint64_t hash = 5381;
	char c = 0;

	while (c == *a_value++)
		hash = ((hash << 5) + hash) + static_cast<unsigned char>(c);

	return hash;
}
