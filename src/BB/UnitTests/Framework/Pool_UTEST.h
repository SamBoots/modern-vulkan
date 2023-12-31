#pragma once
#include "../TestValues.h"
#include "Storage/Pool.h"
#include "Storage/GrowPool.h"
#include "Storage/RawPool.hpp"

TEST(PoolDataStructure, Pool_Create_Get_Free)
{
	constexpr const size_t samples = 128;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	//2 MB alloactor.
	BB::FreelistAllocator_t t_Allocator(1024 * 1024 * 2);

	BB::Pool<size2593bytes> t_Pool;
	t_Pool.CreatePool(t_Allocator, samples);

	size_t t_RandomValues[samples]{};
	size2593bytes* t_Array[samples]{};

	for (size_t i = 0; i < samples; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
		t_Array[i] = t_Pool.Get();

		//If the pool is empty it returns a nullptr, so it must not return a nullptr here.
		EXPECT_NE(t_Array[i], nullptr);
		t_Array[i]->value = t_RandomValues[i];
	}

	for (size_t i = 0; i < samples; i++)
	{
		EXPECT_EQ(t_Array[i]->value, t_RandomValues[i]);
	}

	//The pool is now empty so when Get gets called again it must return a nullptr.
	EXPECT_EQ(t_Pool.Get(), nullptr);

	//Free the entire pool one by one and try again.
	for (size_t i = 0; i < samples; i++)
	{
		t_Pool.Free(t_Array[i]);
	}

	for (size_t i = 0; i < samples; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
		t_Array[i] = t_Pool.Get();

		//If the pool is empty it returns a nullptr, so it must not return a nullptr here.
		EXPECT_NE(t_Array[i], nullptr);
		t_Array[i]->value = t_RandomValues[i];
	}

	for (size_t i = 0; i < samples; i++)
	{
		EXPECT_EQ(t_Array[i]->value, t_RandomValues[i]);
	}

	//The pool is now empty so when Get gets called again it must return a nullptr.
	EXPECT_EQ(t_Pool.Get(), nullptr);

	t_Pool.DestroyPool(t_Allocator);
}


TEST(GrowPoolDataStructure, GrowPool_Create_Get_Free)
{
	constexpr const size_t samples = 128;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	BB::GrowPool<size2593bytes> t_Pool;
	t_Pool.CreatePool(samples);

	size_t t_RandomValues[samples]{};
	size2593bytes* t_Array[samples]{};

	for (size_t i = 0; i < samples; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
		t_Array[i] = t_Pool.Get();

		//If the pool is empty it returns a nullptr, so it must not return a nullptr here.
		EXPECT_NE(t_Array[i], nullptr);
		t_Array[i]->value = t_RandomValues[i];
	}

	for (size_t i = 0; i < samples; i++)
	{
		EXPECT_EQ(t_Array[i]->value, t_RandomValues[i]);
	}

	//The pool is now empty, but because it's a grow pool we should get something.
	EXPECT_NE(t_Pool.Get(), nullptr);

	//Free the entire pool one by one and try again.
	for (size_t i = 0; i < samples; i++)
	{
		t_Pool.Free(t_Array[i]);
	}

	for (size_t i = 0; i < samples; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
		t_Array[i] = t_Pool.Get();

		//If the pool is empty it returns a nullptr, so it must not return a nullptr here.
		EXPECT_NE(t_Array[i], nullptr);
		t_Array[i]->value = t_RandomValues[i];
	}

	for (size_t i = 0; i < samples; i++)
	{
		EXPECT_EQ(t_Array[i]->value, t_RandomValues[i]);
	}

	//The pool is now empty, but because it's a grow pool we should get something.
	EXPECT_NE(t_Pool.Get(), nullptr);

	t_Pool.DestroyPool();
}

TEST(PoolDataStructure, Raw_Pool_Create_Get_Free)
{
	constexpr const size_t samples = 800;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	//2 MB alloactor.
	BB::FreelistAllocator_t t_Allocator(1024 * 1024 * 2);

	BB::RawPool<size2593bytes> pool{ t_Allocator, samples };

	size_t t_RandomValues[samples]{};
	size2593bytes* t_Array[samples]{};

	for (size_t i = 0; i < samples; i++)
	{
		t_RandomValues[i] = static_cast<size_t>(BB::Random::Random());
		t_Array[i] = pool.Get();

		//If the pool is empty it returns a nullptr, so it must not return a nullptr here.
		EXPECT_NE(t_Array[i], nullptr);
		t_Array[i]->value = t_RandomValues[i];
	}

	for (size_t i = 0; i < samples; i++)
	{
		EXPECT_EQ(t_Array[i]->value, t_RandomValues[i]);
	}

	//The pool is now empty so when Get gets called again it must return a nullptr.
	EXPECT_EQ(pool.Get(), nullptr);
}