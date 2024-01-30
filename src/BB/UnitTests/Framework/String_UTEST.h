#pragma once
#include "../TestValues.h"
#include "Storage/BBString.h"

#pragma region WString
TEST(String_DataStructure, append_insert_push_pop_copy_assignment)
{
	constexpr size_t StringReserve = 128;
	constexpr const char* TestString = "First Unit test of the string is now being done.";

	const size_t allocatorSize = BB::kbSize * 16;
	BB::MemoryArena arena = BB::MemoryArenaCreate();

	BB::String string(arena);

	//test single string and compare.
	{
		string.append(TestString);

		EXPECT_EQ(string.compare(TestString), true) << "Append(const char*) failed";
	}

	string.clear();

	//test single string and compare, this time from a BB::String
	{
		BB::String t_TestString(arena, TestString);

		string.append(t_TestString);

		EXPECT_EQ(string.compare(t_TestString), true) << "Append(const char*) failed";
	}

	string.clear();

#pragma region append Test
	//test break the string apart and use parts of it.
	{
		const char* t_AppendString = "First Unit test of the string is now being done.";
		string.append(t_AppendString, 10);

		EXPECT_EQ(string.compare("First Unit"), true) << "Append(const char*, size_t) failed";

		string.append(t_AppendString + string.size(), 12);

		EXPECT_EQ(string.compare(10, " test of the"), true) << "Append(const char*, size_t) failed second call";

		string.append(t_AppendString + string.size(), 26);

		EXPECT_EQ(string.compare(t_AppendString), true) << "Append(const char*, size_t) failed third call";
	}

	string.clear();

	//test break the string apart and use parts of it. Now using a string.
	{
		BB::String t_AppendString(arena, "First Unit test of the string is now being done.");
		string.append(t_AppendString, 0, 10);

		EXPECT_EQ(string.compare("First Unit"), true) << "Append(BB::String&, size_t, size_t) failed";

		string.append(t_AppendString, string.size(), 12);

		EXPECT_EQ(string.compare(10, " test of the"), true) << "Append(BB::String&, size_t, size_t) failed second call";

		string.append(t_AppendString, string.size(), 26);

		EXPECT_EQ(string.compare(t_AppendString), true) << "Append(BB::String&, size_t, size_t) failed third call";
	}
#pragma endregion //append Test

	string.clear();

#pragma region insert Test
	//test break the string apart and use parts of it
	{
		constexpr const char* a_One = "One";
		constexpr const char* a_Two = "Two";
		constexpr const char* a_Three = "Three";
		constexpr const char* a_Four = "Four";

		string.insert(0, a_Four);
		EXPECT_EQ(string.compare("Four"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(0, a_Two);
		EXPECT_EQ(string.compare("TwoFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(3, a_One);
		EXPECT_EQ(string.compare("TwoOneFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(6, a_Three);
		EXPECT_EQ(string.compare("TwoOneThreeFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	}

	string.clear();

	//test break the string apart and use parts of it. Now using a string.
	{
		BB::String t_AppendString(arena, "One, Two, Three, Four");
		string.insert(0, t_AppendString, 17, 4);
		EXPECT_EQ(string.compare("Four"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
		
		string.insert(0, t_AppendString, 5, 3);
		EXPECT_EQ(string.compare("TwoFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(3, t_AppendString, 0, 3);
		EXPECT_EQ(string.compare("TwoOneFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	
		string.insert(6, t_AppendString, 10, 5);
		EXPECT_EQ(string.compare("TwoOneThreeFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	}
#pragma endregion //insert Test
	BB::MemoryArenaFree(arena);
}

#pragma endregion //String

#pragma region WString
TEST(WideString_DataStructure, append_insert_push_pop_copy_assignment)
{
	constexpr size_t StringReserve = 128;
	constexpr const wchar_t* TestString = L"First Unit test of the string is now being done.";

	BB::MemoryArena arena = BB::MemoryArenaCreate();

	BB::WString string(arena);

	//test single string and compare.
	{
		string.append(TestString);

		EXPECT_EQ(string.compare(TestString), true) << "Append(const char*) failed";
	}

	string.clear();

	//test single string and compare, this time from a BB::String
	{
		BB::WString test_string(arena, TestString);
		string.append(test_string);

		EXPECT_EQ(string.compare(test_string), true) << "Append(const char*) failed";
	}

	string.clear();

#pragma region append Test
	//test break the string apart and use parts of it.
	{
		const wchar_t* t_AppendString = L"First Unit test of the string is now being done.";
		string.append(t_AppendString, 10);

		EXPECT_EQ(string.compare(L"First Unit"), true) << "Append(const char*, size_t) failed";

		string.append(t_AppendString + string.size(), 12);

		EXPECT_EQ(string.compare(10, L" test of the"), true) << "Append(const char*, size_t) failed second call";

		string.append(t_AppendString + string.size(), 26);

		EXPECT_EQ(string.compare(t_AppendString), true) << "Append(const char*, size_t) failed third call";
	}

	string.clear();

	//test break the string apart and use parts of it. Now using a string.
	{
		BB::WString t_AppendString(arena, L"First Unit test of the string is now being done.");
		string.append(t_AppendString, 0, 10);

		EXPECT_EQ(string.compare(L"First Unit"), true) << "Append(BB::String&, size_t, size_t) failed";

		string.append(t_AppendString, string.size(), 12);

		EXPECT_EQ(string.compare(10, L" test of the"), true) << "Append(BB::String&, size_t, size_t) failed second call";

		string.append(t_AppendString, string.size(), 26);

		EXPECT_EQ(string.compare(t_AppendString), true) << "Append(BB::String&, size_t, size_t) failed third call";
	}
#pragma endregion //append Test

	string.clear();

#pragma region insert Test
	//test break the string apart and use parts of it
	{
		constexpr const wchar_t* a_One = L"One";
		constexpr const wchar_t* a_Two = L"Two";
		constexpr const wchar_t* a_Three = L"Three";
		constexpr const wchar_t* a_Four = L"Four";

		string.insert(0, a_Four);
		EXPECT_EQ(string.compare(L"Four"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(0, a_Two);
		EXPECT_EQ(string.compare(L"TwoFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(3, a_One);
		EXPECT_EQ(string.compare(L"TwoOneFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(6, a_Three);
		EXPECT_EQ(string.compare(L"TwoOneThreeFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	}

	string.clear();

	//test break the string apart and use parts of it. Now using a string.
	{
		BB::WString t_AppendString(arena, L"One, Two, Three, Four");
		string.insert(0, t_AppendString, 17, 4);
		EXPECT_EQ(string.compare(L"Four"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(0, t_AppendString, 5, 3);
		EXPECT_EQ(string.compare(L"TwoFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(3, t_AppendString, 0, 3);
		EXPECT_EQ(string.compare(L"TwoOneFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";

		string.insert(6, t_AppendString, 10, 5);
		EXPECT_EQ(string.compare(L"TwoOneThreeFour"), true) << "insert(size_t, BB::String&, size_t, size_t) failed";
	}
#pragma endregion //insert Test

	BB::MemoryArenaFree(arena);
}

#pragma endregion //WString