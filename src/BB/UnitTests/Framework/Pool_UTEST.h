#pragma once
#include "../TestValues.h"
#include "Storage/Pool.h"
#include "Storage/GrowPool.h"

TEST(PoolDataStructure, Pool_Create_Get_Free)
{
	constexpr const size_t samples = 128;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	BB::MemoryArena arena = BB::MemoryArenaCreate();
	BB::Pool<size2593bytes> pool;
	pool.CreatePool(arena, samples);

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

	//Free the entire pool one by one and try again.
	for (size_t i = 0; i < samples; i++)
	{
		pool.Free(t_Array[i]);
	}

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

	BB::MemoryArenaFree(arena);
}


TEST(GrowPoolDataStructure, GrowPool_Create_Get_Free)
{
	constexpr const size_t samples = 128;
	//Unaligned big struct with a union to test the value.
	struct size2593bytes { union { char data[2593]; size_t value; }; };

	BB::GrowPool<size2593bytes> pool;
	pool.CreatePool(samples);

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

	//The pool is now empty, but because it's a grow pool we should get something.
	EXPECT_NE(pool.Get(), nullptr);

	//Free the entire pool one by one and try again.
	for (size_t i = 0; i < samples; i++)
	{
		pool.Free(t_Array[i]);
	}

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

	//The pool is now empty, but because it's a grow pool we should get something.
	EXPECT_NE(pool.Get(), nullptr);

	pool.DestroyPool();
}
