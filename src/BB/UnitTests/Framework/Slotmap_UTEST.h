#pragma once
#include "../TestValues.h"
#include "Storage/Slotmap.h"

TEST(Slotmap_Datastructure, Slotmap_Insert_Remove)
{
	constexpr const size_t samples = 128;

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::Slotmap<size2593bytesObj> t_Map(t_Allocator, samples);

	{
		size2593bytesObj t_Value1{};
		t_Value1.value = 500;
		BB::SlotmapHandle id1 = t_Map.insert(t_Value1);
		ASSERT_EQ(t_Map.find(id1).value, t_Value1.value) << "Wrong element was likely grabbed.";

		//try inserting again after an deletion.
		size2593bytesObj t_Value2{};
		t_Value2.value = 1000;
		BB::SlotmapHandle id2 = t_Map.insert(t_Value2);
		ASSERT_EQ(t_Map.find(id2).value, t_Value2.value) << "Wrong element was likely grabbed.";

		t_Map.erase(id1);

		ASSERT_EQ(t_Map.find(id2).value, t_Value2.value) << "Wrong element was likely grabbed.";

		id1 = t_Map.insert(t_Value1);
		ASSERT_EQ(t_Map.find(id1).value, t_Value1.value) << "Wrong element was likely grabbed.";
	}
}

TEST(Slotmap_Datastructure, Slotmap_Insert_Erase_Iterator)
{
	constexpr const size_t samples = 128;

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::Slotmap<size2593bytesObj> t_Map(t_Allocator, samples);

	{
		size2593bytesObj t_Value{};
		t_Value.value = 500;
		BB::SlotmapHandle id = t_Map.insert(t_Value);
		ASSERT_EQ(t_Map.find(id).value, t_Value.value) << "Wrong element was likely grabbed.";

		t_Map.erase(id);

		//try inserting again after an deletion.
		id = t_Map.insert(t_Value);
		ASSERT_EQ(t_Map.find(id).value, t_Value.value) << "Wrong element was likely grabbed.";

		t_Map.erase(id);
	}

	size2593bytesObj t_RandomKeys[samples]{};
	BB::SlotmapHandle ids[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		t_RandomKeys[i] = static_cast<size_t>(BB::Random::Random());
	}

	for (size_t i = 0; i < samples; i++)
	{
		ids[i] = t_Map.insert(t_RandomKeys[i]);
	}


	size_t t_Count = 0;
	for (auto it = t_Map.begin(); it < t_Map.end(); it++)
	{
		ASSERT_EQ(it->value, t_RandomKeys[t_Count++].value) << "Wrong element was likely grabbed.";
	}
	ASSERT_EQ(samples, t_Count) << "Iterator went over the sample amount.";

	t_Map.clear();

	for (size_t i = 0; i < samples; i++)
	{
		ids[i] = t_Map.insert(t_RandomKeys[i]);
	}

	t_Count = 0;
	for (auto it = t_Map.begin(); it < t_Map.end(); it++)
	{
		ASSERT_EQ(it->value, t_RandomKeys[t_Count++].value) << "Wrong element was likely grabbed.";
	}
	ASSERT_EQ(samples, t_Count) << "Iterator went over the sample amount.";
}

TEST(Slotmap_Datastructure, Slotmap_Copy_Assignment)
{
	constexpr const size_t samples = 128;

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::Slotmap<size2593bytesObj> t_Map(t_Allocator, samples);

	size2593bytesObj t_RandomKeys[samples]{};
	BB::SlotmapHandle ids[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		t_RandomKeys[i] = static_cast<size_t>(BB::Random::Random());
	}

	for (size_t i = 0; i < samples; i++)
	{
		ids[i] = t_Map.insert(t_RandomKeys[i]);
	}


	size_t t_Count = 0;
	for (auto it = t_Map.begin(); it < t_Map.end(); it++)
	{
		ASSERT_EQ(it->value, t_RandomKeys[t_Count++].value) << "Wrong element was likely grabbed.";
	}
	ASSERT_EQ(samples, t_Count) << "Iterator went over the sample amount.";

	BB::Slotmap<size2593bytesObj> t_CopyMap(t_Map);

	t_Count = 0;
	for (auto it = t_CopyMap.begin(); it < t_CopyMap.end(); it++)
	{
		ASSERT_EQ(it->value, t_RandomKeys[t_Count++].value) << "Wrong element was likely grabbed.";
	}
	ASSERT_EQ(samples, t_Count) << "Iterator went over the sample amount.";

	BB::Slotmap<size2593bytesObj> t_CopyOperatorMap(t_CopyMap);
	t_CopyOperatorMap = t_CopyMap;

	t_Count = 0;
	for (auto it = t_CopyOperatorMap.begin(); it < t_CopyOperatorMap.end(); it++)
	{
		ASSERT_EQ(it->value, t_RandomKeys[t_Count++].value) << "Wrong element was likely grabbed.";
	}
	ASSERT_EQ(samples, t_Count) << "Iterator went over the sample amount.";

	BB::Slotmap<size2593bytesObj> t_AssignmentMap(std::move(t_CopyOperatorMap));
	ASSERT_EQ(t_CopyOperatorMap.size(), 0) << "The map that was moved is not 0 in size.";

	t_Count = 0;
	for (auto it = t_AssignmentMap.begin(); it < t_AssignmentMap.end(); it++)
	{
		ASSERT_EQ(it->value, t_RandomKeys[t_Count++].value) << "Wrong element was likely grabbed.";
	}
	ASSERT_EQ(samples, t_Count) << "Iterator went over the sample amount.";

	BB::Slotmap<size2593bytesObj> t_AssignmentOperatorMap(t_Allocator);
	t_AssignmentOperatorMap = std::move(t_AssignmentMap);

	ASSERT_EQ(t_AssignmentMap.size(), 0) << "The map that was moved is not 0 in size.";

	t_Count = 0;
	for (auto it = t_AssignmentOperatorMap.begin(); it < t_AssignmentOperatorMap.end(); it++)
	{
		ASSERT_EQ(it->value, t_RandomKeys[t_Count++].value) << "Wrong element was likely grabbed.";
	}
	ASSERT_EQ(samples, t_Count) << "Iterator went over the sample amount.";
}

TEST(Slotmap_Datastructure, Slotmap_Reserve_Grow)
{
	constexpr const size_t samples = 128;
	constexpr const size_t initialMapSize = 16;

	//32 MB alloactor.
	const size_t allocatorSize = BB::mbSize * 32;
	BB::FreelistAllocator_t t_Allocator(allocatorSize);

	BB::Slotmap<size2593bytesObj> t_Map(t_Allocator);
	t_Map.reserve(initialMapSize);

	size2593bytesObj t_RandomKeys[samples]{};
	BB::SlotmapHandle ids[samples]{};
	for (size_t i = 0; i < samples; i++)
	{
		t_RandomKeys[i] = static_cast<size_t>(BB::Random::Random());
	}

	for (size_t i = 0; i < initialMapSize; i++)
	{
		ids[i] = t_Map.insert(t_RandomKeys[i]);
	}


	size_t t_Count = 0;
	for (auto it = t_Map.begin(); it < t_Map.end(); it++)
	{
		ASSERT_EQ(it->value, t_RandomKeys[t_Count++].value) << "Wrong element was likely grabbed.";
	}
	ASSERT_EQ(initialMapSize, t_Count) << "Iterator went over the initialMapSize amount.";

	for (size_t i = initialMapSize; i < samples; i++)
	{
		ids[i] = t_Map.insert(t_RandomKeys[i]);
	}

	t_Count = 0;
	for (auto it = t_Map.begin(); it < t_Map.end(); it++)
	{
		ASSERT_EQ(it->value, t_RandomKeys[t_Count++].value) << "Wrong element was likely grabbed after a grow event.";
	}
	ASSERT_EQ(samples, t_Count) << "Iterator went over the sample after a grow event amount.";
}