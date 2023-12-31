#pragma once
#include "../TestValues.h"
#include "Allocators_UTEST.h"
#include <chrono>

using namespace BB;

static void FillBuffer(uint8_t* a_value, size_t a_size)
{
	const size_t t_Sequences = a_size / 8;
	size_t* t_ModifiedValues = reinterpret_cast<size_t*>(a_value);
	t_ModifiedValues[0] = 0;
	t_ModifiedValues[1] = 1;

	for (size_t i = 2; i < t_Sequences; i++)
	{
		size_t t_Value = t_ModifiedValues[i - 1] + t_ModifiedValues[i - 2];
		t_ModifiedValues[i] = t_Value;
	}
}


TEST(MemoryOperation_Speed_Comparison, Memcpy_Aligned)
{
	typedef std::chrono::duration<float, std::milli> ms;
	constexpr const float MILLITIMEDIVIDE = 1 / 1000.f;

	constexpr size_t SmallCopySize = 256;
	constexpr size_t MediumCopySize = mbSize;
	constexpr size_t BigCopySize = gbSize / 2;

	BB::FixedLinearAllocator_t t_FixedAllocator(SmallCopySize +
		MediumCopySize +
		BigCopySize * 2);

	uint8_t* smallBuffer = BBnewArr(t_FixedAllocator, SmallCopySize, uint8_t);
	FillBuffer(smallBuffer, SmallCopySize);
	uint8_t* mediumBuffer = BBnewArr(t_FixedAllocator, MediumCopySize, uint8_t);
	FillBuffer(mediumBuffer, MediumCopySize);
	uint8_t* bigBuffer = BBnewArr(t_FixedAllocator, BigCopySize, uint8_t);
	FillBuffer(bigBuffer, BigCopySize);

	uint8_t* copyBuffer = BBnewArr(t_FixedAllocator, BigCopySize, uint8_t);
	FillBuffer(copyBuffer, BigCopySize);



	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpy(copyBuffer, smallBuffer, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 256 bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpySIMD128(copyBuffer, smallBuffer, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 256 bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpySIMD256(copyBuffer, smallBuffer, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 256 bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		memcpy(copyBuffer, smallBuffer, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 256 bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpy(copyBuffer, mediumBuffer, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 1MB Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpySIMD128(copyBuffer, mediumBuffer, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 1MB bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpySIMD256(copyBuffer, mediumBuffer, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 1MB bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		memcpy(copyBuffer, mediumBuffer, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 1MB Memcpy Speed in MS:" << t_Speed << "\n";
	}

	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpy(copyBuffer, bigBuffer, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 512MB Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpySIMD128(copyBuffer, bigBuffer, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 512MB bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemCpySIMD256(copyBuffer, bigBuffer, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 512MB bytes Memcpy Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		memcpy(copyBuffer, bigBuffer, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 512MB Memcpy Speed in MS:" << t_Speed << "\n";
	}

	t_FixedAllocator.Clear();
}

TEST(MemoryOperation_Speed_Comparison, MemCmp_Aligned)
{
	typedef std::chrono::duration<float, std::milli> ms;
	constexpr const float MILLITIMEDIVIDE = 1 / 1000.f;

	constexpr const int32_t TESTVALUE = 5215;

	constexpr size_t SmallCopySize = 256;
	constexpr size_t MediumCopySize = mbSize;
	constexpr size_t BigCopySize = gbSize / 2;

	BB::FixedLinearAllocator_t t_FixedAllocator((SmallCopySize +
		MediumCopySize +
		BigCopySize) * 4);

	uint8_t* smallBuffer = BBnewArr(t_FixedAllocator, SmallCopySize, uint8_t);
	uint8_t* smallCmprBuffer = BBnewArr(t_FixedAllocator, SmallCopySize, uint8_t);
	FillBuffer(smallBuffer, SmallCopySize);
	FillBuffer(smallCmprBuffer, SmallCopySize);
	smallCmprBuffer[SmallCopySize - 10] = 255;

	uint8_t* mediumBuffer = BBnewArr(t_FixedAllocator, MediumCopySize, uint8_t);
	uint8_t* mediumCmprBuffer = BBnewArr(t_FixedAllocator, MediumCopySize, uint8_t);
	FillBuffer(mediumBuffer, MediumCopySize);
	FillBuffer(mediumCmprBuffer, MediumCopySize);
	mediumCmprBuffer[MediumCopySize - 10] = 255;

	uint8_t* bigBuffer = BBnewArr(t_FixedAllocator, BigCopySize, uint8_t);
	uint8_t* bigCmprBuffer = BBnewArr(t_FixedAllocator, BigCopySize, uint8_t);
	FillBuffer(bigBuffer, BigCopySize);
	FillBuffer(bigCmprBuffer, BigCopySize);
	bigCmprBuffer[BigCopySize - 10] = 255;

#pragma region SmallBuff
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

	 	EXPECT_EQ(BB::Memory::MemCmp(smallBuffer, smallBuffer, SmallCopySize), true);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 256 equal bytes memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmp(smallBuffer, smallCmprBuffer, SmallCopySize), false);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 256 unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD128(smallBuffer, smallBuffer, SmallCopySize), true);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 256 equal bytes memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD128(smallBuffer, smallCmprBuffer, SmallCopySize), false);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 256 unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD256(smallBuffer, smallBuffer, SmallCopySize), true);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 256 equal bytes memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD256(smallBuffer, smallCmprBuffer, SmallCopySize), false);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 256 unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(memcmp(smallBuffer, smallBuffer, SmallCopySize), 0);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 256 equal bytes memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_NE(memcmp(smallBuffer, smallCmprBuffer, SmallCopySize), 0);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 256 unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
#pragma endregion //SmallBuff

#pragma region MediumBuff
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmp(mediumBuffer, mediumBuffer, MediumCopySize), true);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 1MB equal memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmp(mediumBuffer, mediumCmprBuffer, MediumCopySize), false);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 1MB unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD128(mediumBuffer, mediumBuffer, MediumCopySize), true);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 1MB equal memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD128(mediumBuffer, mediumCmprBuffer, MediumCopySize), false);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 1MB unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD256(mediumBuffer, mediumBuffer, MediumCopySize), true);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 1MB equal memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD256(mediumBuffer, mediumCmprBuffer, MediumCopySize), false);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 1MB unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(memcmp(mediumBuffer, mediumBuffer, MediumCopySize), 0);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 1MB equal memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_NE(memcmp(mediumBuffer, mediumCmprBuffer, MediumCopySize), 0);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 1MB unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
#pragma endregion //MediumBuff

#pragma region BigBuff
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmp(bigBuffer, bigBuffer, BigCopySize), true);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 512MB equal memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmp(bigBuffer, bigCmprBuffer, BigCopySize), false);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 512MB unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD128(bigBuffer, bigBuffer, BigCopySize), true);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 512MB equal memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD128(bigBuffer, bigCmprBuffer, BigCopySize), false);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 512MB unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD256(bigBuffer, bigBuffer, BigCopySize), true);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 512MB equal memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(BB::Memory::MemCmpSIMD256(bigBuffer, bigCmprBuffer, BigCopySize), false);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 512MB unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_EQ(memcmp(bigBuffer, bigBuffer, BigCopySize), 0);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 512MB equal memcmp Speed in MS:" << t_Speed << "\n";

		t_Timer = std::chrono::high_resolution_clock::now();

		EXPECT_NE(memcmp(bigBuffer, bigCmprBuffer, BigCopySize), 0);

		t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 512MB unequal bytes memcmp Speed in MS:" << t_Speed << "\n";
	}
#pragma endregion //BigBuff

	t_FixedAllocator.Clear();
}

TEST(MemoryOperation_Speed_Comparison, MemSet_Aligend)
{
	typedef std::chrono::duration<float, std::milli> ms;
	constexpr const float MILLITIMEDIVIDE = 1 / 1000.f;

	constexpr const int32_t TESTVALUE = 5215;

	constexpr size_t SmallCopySize = 256;
	constexpr size_t MediumCopySize = mbSize;
	constexpr size_t BigCopySize = gbSize / 2;

	BB::FixedLinearAllocator_t t_FixedAllocator(SmallCopySize +
		MediumCopySize +
		BigCopySize * 2);

	uint8_t* smallBuffer = BBnewArr(t_FixedAllocator, SmallCopySize, uint8_t);
	uint8_t* mediumBuffer = BBnewArr(t_FixedAllocator, MediumCopySize, uint8_t);
	uint8_t* bigBuffer = BBnewArr(t_FixedAllocator, BigCopySize, uint8_t);

#pragma region SmallBuff
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemSet(smallBuffer, TESTVALUE, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 256 bytes memset Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemSetSIMD128(smallBuffer, TESTVALUE, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 256 bytes memset Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemSetSIMD256(smallBuffer, TESTVALUE, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 256 bytes memset Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		memset(smallBuffer, TESTVALUE, SmallCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 256 bytes memset Speed in MS:" << t_Speed << "\n";
	}
#pragma endregion //SmallBuff

#pragma region MediumBuff
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemSet(mediumBuffer, TESTVALUE, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 1MB memset Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemSetSIMD128(mediumBuffer, TESTVALUE, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 1MB memset Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemSetSIMD256(mediumBuffer, TESTVALUE, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 1MB memset Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		memset(mediumBuffer, TESTVALUE, MediumCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 1MB memset Speed in MS:" << t_Speed << "\n";
	}
#pragma endregion //MediumBuff

#pragma region BigBuff
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemSet(bigBuffer, TESTVALUE, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB 512MB memset Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemSetSIMD128(bigBuffer, TESTVALUE, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD128 512MB memset Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		BB::Memory::MemSetSIMD256(bigBuffer, TESTVALUE, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "BB SIMD256 512MB memset Speed in MS:" << t_Speed << "\n";
	}
	{
		auto t_Timer = std::chrono::high_resolution_clock::now();

		memset(bigBuffer, TESTVALUE, BigCopySize);

		auto t_Speed = std::chrono::duration_cast<ms>(std::chrono::high_resolution_clock::now() - t_Timer).count() * MILLITIMEDIVIDE;
		std::cout << "STL 512MB memset Speed in MS:" << t_Speed << "\n";
	}
#pragma endregion //BigBuff

	t_FixedAllocator.Clear();
}