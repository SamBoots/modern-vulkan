#pragma once
#include "Utils/Hash.h"
#include "Utils/Utils.h"
#include "BBMemory.h"
namespace BB
{
	namespace Hashmap_Specs
	{
		constexpr uint32_t Standard_Hashmap_Size = 64;

		constexpr const size_t multipleValue = 8;

		constexpr const float UM_LoadFactor = 1.f;
		constexpr const size_t UM_EMPTYNODE = 0xAABBCCDD;

		constexpr const float OL_LoadFactor = 1.3f;
		constexpr const size_t OL_TOMBSTONE = 0xDEADBEEFDEADBEEF;
		constexpr const size_t OL_EMPTY = 0xAABBCCDD;
	};

	//Calculate the load factor.
	static size_t LFCalculation(size_t a_size, float a_LoadFactor)
	{
		return static_cast<size_t>(static_cast<float>(a_size) * (1.f / a_LoadFactor + 1.f));
	}

	struct String_KeyComp
	{
		bool operator()(const char* a_a, const char* a_b) const
		{
			return strcmp(a_a, a_b) == 0;
		}
	};

	template<typename Key>
	struct Standard_KeyComp
	{
		inline bool operator()(const Key a_a, const Key a_b) const
		{
			return a_a == a_b;
		}
	};

#pragma region Unordered_Map
	//Unordered Map, uses linked list for collision.
	template<typename Key, typename Value, typename KeyComp = Standard_KeyComp<Key>>
	class UM_HashMap
	{
		struct HashEntry
		{
			static constexpr bool trivalDestructableKey = std::is_trivially_destructible_v<Key>;
			static constexpr bool trivalDestructableValue = std::is_trivially_destructible_v<Value>;
			~HashEntry()
			{
				state = Hashmap_Specs::UM_EMPTYNODE;
				//Call the destructor if it has one for the value.
				if constexpr (!trivalDestructableValue)
					value.~Value();
				//Call the destructor if it has one for the key.
				if constexpr (!trivalDestructableKey)
					key.~Key();
			}
			HashEntry* next_Entry = nullptr;
			union
			{
				size_t state = Hashmap_Specs::UM_EMPTYNODE;
				Key key;
			};
			Value value;
		};

	public:
		UM_HashMap(Allocator a_allocator)
			: UM_HashMap(a_allocator, Hashmap_Specs::Standard_Hashmap_Size)
		{}
		UM_HashMap(Allocator a_allocator, const size_t a_size)
			: m_allocator(a_allocator)
		{
			m_capacity = LFCalculation(a_size, Hashmap_Specs::UM_LoadFactor);
			m_size = 0;
			m_LoadCapacity = a_size;
			m_Entries = reinterpret_cast<HashEntry*>(BBalloc(m_allocator, m_capacity * sizeof(HashEntry)));

			for (size_t i = 0; i < m_capacity; i++)
			{
				new (&m_Entries[i]) HashEntry();
			}
		}
		UM_HashMap(const UM_HashMap<Key, Value>& a_map)
		{
			m_allocator = a_map.m_allocator;
			m_size = a_map.m_size;
			m_capacity = a_map.m_capacity;
			m_LoadCapacity = a_map.m_LoadCapacity;

			m_Entries = reinterpret_cast<HashEntry*>(BBalloc(m_allocator, m_capacity * sizeof(HashEntry)));

			//Copy over the hashmap and construct the element
			for (size_t i = 0; i < m_capacity; i++)
			{
				if (a_map.m_Entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
				{
					new (&m_Entries[i]) HashEntry(a_map.m_Entries[i]);
					HashEntry* t_Entry = &m_Entries[i];
					HashEntry* t_PreviousEntry;
					while (t_Entry->next_Entry != nullptr)
					{
						t_PreviousEntry = t_Entry;
						t_Entry = reinterpret_cast<HashEntry*>(BBnew(m_allocator, HashEntry)(*t_Entry->next_Entry));
						t_PreviousEntry->next_Entry = t_Entry;
					}
				}
				else
				{
					new (&m_Entries[i]) HashEntry();
				}
			}
		}
		UM_HashMap(UM_HashMap<Key, Value>&& a_map) noexcept
		{
			m_allocator = a_map.m_allocator;
			m_size = a_map.m_size;
			m_capacity = a_map.m_capacity;
			m_LoadCapacity = a_map.m_LoadCapacity;
			m_Entries = a_map.m_Entries;

			a_map.m_size = 0;
			a_map.m_capacity = 0;
			a_map.m_LoadCapacity = 0;
			a_map.m_Entries = nullptr;
			a_map.m_allocator.allocator = nullptr;
			a_map.m_allocator.func = nullptr;
		}
		~UM_HashMap()
		{
			if (m_Entries != nullptr)
			{
				clear();

				BBfree(m_allocator, m_Entries);
			}
		}

		UM_HashMap<Key, Value>& operator=(const UM_HashMap<Key, Value>& a_rhs)
		{
			this->~UM_HashMap();

			m_allocator = a_rhs.m_allocator;
			m_size = a_rhs.m_size;
			m_capacity = a_rhs.m_capacity;
			m_LoadCapacity = a_rhs.m_LoadCapacity;

			m_Entries = reinterpret_cast<HashEntry*>(BBalloc(m_allocator, m_capacity * sizeof(HashEntry)));

			//Copy over the hashmap and construct the element
			for (size_t i = 0; i < m_capacity; i++)
			{
				if (a_rhs.m_Entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
				{
					new (&m_Entries[i]) HashEntry(a_rhs.m_Entries[i]);
					HashEntry* t_Entry = &m_Entries[i];
					HashEntry* t_PreviousEntry;
					while (t_Entry->next_Entry != nullptr)
					{
						t_PreviousEntry = t_Entry;
						t_Entry = reinterpret_cast<HashEntry*>(BBnew(m_allocator, HashEntry)(*t_Entry->next_Entry));
						t_PreviousEntry->next_Entry = t_Entry;
					}
				}
				else
				{
					new (&m_Entries[i]) HashEntry();
				}
			}

			return *this;
		}
		UM_HashMap<Key, Value>& operator=(UM_HashMap<Key, Value>&& a_rhs) noexcept
		{
			this->~UM_HashMap();

			m_allocator = a_rhs.m_allocator;
			m_size = a_rhs.m_size;
			m_capacity = a_rhs.m_capacity;
			m_LoadCapacity = a_rhs.m_LoadCapacity;
			m_Entries = a_rhs.m_Entries;

			a_rhs.m_size = 0;
			a_rhs.m_capacity = 0;
			a_rhs.m_LoadCapacity = 0;
			a_rhs.m_Entries = nullptr;
			a_rhs.m_allocator.allocator = nullptr;
			a_rhs.m_allocator.func = nullptr;

			return *this;
		}

		void insert(const Key& a_Key, Value& a_Res)
		{
			emplace(a_Key, a_Res);
		}
		template <class... Args>
		void emplace(const Key& a_Key, Args&&... a_ValueArgs)
		{
			if (m_size > m_LoadCapacity)
				grow();

			const Hash t_Hash = Hash::MakeHash(a_Key) % m_capacity;

			HashEntry* t_Entry = &m_Entries[t_Hash.hash];
			if (t_Entry->state == Hashmap_Specs::UM_EMPTYNODE)
			{
				t_Entry->key = a_Key;
				new (&t_Entry->value) Value(std::forward<Args>(a_ValueArgs)...);
				t_Entry->next_Entry = nullptr;
				return;
			}
			//Collision accurred, no problem we just create a linked list and make a new element.
			//Bad for cache memory though.
			while (t_Entry)
			{
				if (t_Entry->next_Entry == nullptr)
				{
					HashEntry* t_NewEntry = BBnew(m_allocator, HashEntry);
					t_NewEntry->key = a_Key;
					new (&t_NewEntry->value) Value(std::forward<Args>(a_ValueArgs)...);
					t_NewEntry->next_Entry = nullptr;
					t_Entry->next_Entry = t_NewEntry;
					return;
				}
				t_Entry = t_Entry->next_Entry;
			}
		}
		Value* find(const Key& a_Key) const
		{
			const Hash t_Hash = Hash::MakeHash(a_Key) % m_capacity;

			HashEntry* t_Entry = &m_Entries[t_Hash];

			if (t_Entry->state == Hashmap_Specs::UM_EMPTYNODE)
				return nullptr;

			while (t_Entry)
			{
				if (Match(t_Entry, a_Key))
				{
					return &t_Entry->value;
				}
				t_Entry = t_Entry->next_Entry;
			}
			return nullptr;
		}
		void erase(const Key& a_Key)
		{
			const Hash t_Hash = Hash::MakeHash(a_Key) % m_capacity;;

			HashEntry* t_Entry = &m_Entries[t_Hash];
			if (Match(t_Entry, a_Key))
			{
				t_Entry->~HashEntry();

				if (t_Entry->next_Entry != nullptr)
				{
					HashEntry* t_NextEntry = t_Entry->next_Entry;
					*t_Entry = *t_Entry->next_Entry;
					BBfree(m_allocator, t_NextEntry);
					return;
				}

				t_Entry->state = Hashmap_Specs::UM_EMPTYNODE;
				return;
			}

			HashEntry* t_PreviousEntry = nullptr;

			while (t_Entry)
			{
				if (Match(t_Entry, a_Key))
				{
					t_PreviousEntry = t_Entry->next_Entry;
					BBfree(m_allocator, t_Entry);
					return;
				}
				t_PreviousEntry = t_Entry;
				t_Entry = t_Entry->next_Entry;
			}
		}
		void clear()
		{
			//go through all the entries and individually delete the extra values from the linked list.
			//They need to be deleted seperatly since the memory is somewhere else.
			for (size_t i = 0; i < m_capacity; i++)
			{
				if (m_Entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
				{
					HashEntry* t_NextEntry = m_Entries[i].next_Entry;
					while (t_NextEntry != nullptr)
					{
						HashEntry* t_DeleteEntry = t_NextEntry;
						t_NextEntry = t_NextEntry->next_Entry;
						t_DeleteEntry->~HashEntry();

						BBfree(m_allocator, t_DeleteEntry);
					}
					m_Entries[i].state = Hashmap_Specs::UM_EMPTYNODE;
				}
			}
			for (size_t i = 0; i < m_capacity; i++)
				if (m_Entries[i].state == Hashmap_Specs::UM_EMPTYNODE)
					m_Entries[i].~HashEntry();

			m_size = 0;
		}

		void reserve(const size_t a_size)
		{
			if (a_size > m_capacity)
			{
				size_t t_ModifiedCapacity = RoundUp(a_size, Hashmap_Specs::multipleValue);

				reallocate(t_ModifiedCapacity);
			}
		}

		size_t size() const { return m_size; }

	private:
		void grow(size_t a_MinCapacity = 1)
		{
			BB_WARNING(false, "Resizing an OL_HashMap, this might be a bit slow. Possibly reserve more.", WarningType::OPTIMALIZATION);

			size_t t_ModifiedCapacity = m_capacity * 2;

			if (a_MinCapacity > t_ModifiedCapacity)
				t_ModifiedCapacity = RoundUp(a_MinCapacity, Hashmap_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
		}

		void reallocate(const size_t a_NewLoadCapacity)
		{
			const size_t t_NewCapacity = LFCalculation(a_NewLoadCapacity, Hashmap_Specs::UM_LoadFactor);

			//Allocate the new buffer.
			HashEntry* t_NewEntries = reinterpret_cast<HashEntry*>(BBalloc(m_allocator, t_NewCapacity * sizeof(HashEntry)));

			for (size_t i = 0; i < t_NewCapacity; i++)
			{
				new (&t_NewEntries[i]) HashEntry();
			}

			for (size_t i = 0; i < m_capacity; i++)
			{
				if (m_Entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
				{
					const Hash t_Hash = Hash::MakeHash(m_Entries[i].key) % t_NewCapacity;

					HashEntry* t_Entry = &t_NewEntries[t_Hash.hash];
					if (t_Entry->state == Hashmap_Specs::UM_EMPTYNODE)
					{
						*t_Entry = m_Entries[i];
					}
					//Collision accurred, no problem we just create a linked list and make a new element.
					//Bad for cache memory though.
					while (t_Entry)
					{
						if (t_Entry->next_Entry == nullptr)
						{
							HashEntry* t_NewEntry = BBnew(m_allocator, HashEntry)(m_Entries[i]);
						}
						t_Entry = t_Entry->next_Entry;
					}
				}
			}

			this->~UM_HashMap();

			m_capacity = t_NewCapacity;
			m_LoadCapacity = a_NewLoadCapacity;
			m_Entries = t_NewEntries;
		}

		size_t m_capacity;
		size_t m_LoadCapacity;
		size_t m_size = 0;

		HashEntry* m_Entries;

		Allocator m_allocator;

	private:
		bool Match(const HashEntry* a_Entry, const Key& a_Key) const
		{
			return KeyComp()(a_Entry->key, a_Key);
		}
	};


#pragma endregion

#pragma region Open Addressing Linear Probing (OL)
	//Open addressing with Linear probing.
	template<typename Key, typename Value, typename KeyComp = Standard_KeyComp<Key>>
	class OL_HashMap
	{
		static constexpr bool trivalDestructableValue = std::is_trivially_destructible_v<Value>;
		static constexpr bool trivalDestructableKey = std::is_trivially_destructible_v<Key>;

	public:
		OL_HashMap(Allocator a_allocator)
			: OL_HashMap(a_allocator, Hashmap_Specs::Standard_Hashmap_Size)
		{}
		OL_HashMap(Allocator a_allocator, const size_t a_size)
			: m_allocator(a_allocator)
		{
			m_capacity = LFCalculation(a_size, Hashmap_Specs::OL_LoadFactor);
			m_size = 0;
			m_LoadCapacity = a_size;

			const size_t t_MemorySize = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_capacity;

			void* t_Buffer = BBalloc(m_allocator, t_MemorySize);
			m_Hashes = reinterpret_cast<Hash*>(t_Buffer);
			m_Keys = reinterpret_cast<Key*>(Pointer::Add(t_Buffer, sizeof(Hash) * m_capacity));
			m_Values = reinterpret_cast<Value*>(Pointer::Add(t_Buffer, (sizeof(Hash) + sizeof(Key)) * m_capacity));
			for (size_t i = 0; i < m_capacity; i++)
			{
				m_Hashes[i] = Hashmap_Specs::OL_EMPTY;
			}
		}
		OL_HashMap(const OL_HashMap<Key, Value>& a_map)
		{
			m_capacity = a_map.m_capacity;
			m_size = 0;
			m_LoadCapacity = a_map.m_LoadCapacity;

			m_allocator = a_map.m_allocator;

			const size_t t_MemorySize = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_capacity;

			void* t_Buffer = BBalloc(m_allocator, t_MemorySize);
			m_Hashes = reinterpret_cast<Hash*>(t_Buffer);
			m_Keys = reinterpret_cast<Key*>(Pointer::Add(t_Buffer, sizeof(Hash) * m_capacity));
			m_Values = reinterpret_cast<Value*>(Pointer::Add(t_Buffer, (sizeof(Hash) + sizeof(Key)) * m_capacity));
			for (size_t i = 0; i < m_capacity; i++)
			{
				m_Hashes[i] = Hashmap_Specs::OL_EMPTY;
			}

			for (size_t i = 0; i < m_capacity; i++)
			{
				if (a_map.m_Hashes[i] != Hashmap_Specs::OL_EMPTY && a_map.m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE)
				{
					insert(a_map.m_Keys[i], a_map.m_Values[i]);
				}
			}
		}
		OL_HashMap(OL_HashMap<Key, Value>&& a_map) noexcept
		{
			m_capacity = a_map.m_capacity;
			m_size = a_map.m_size;
			m_LoadCapacity = a_map.m_LoadCapacity;

			m_Hashes = a_map.m_Hashes;
			m_Keys = a_map.m_Keys;
			m_Values = a_map.m_Values;

			m_allocator = a_map.m_allocator;

			a_map.m_capacity = 0;
			a_map.m_size = 0;
			a_map.m_LoadCapacity = 0;
			a_map.m_Hashes = nullptr;
			a_map.m_Keys = nullptr;
			a_map.m_Values = nullptr;

			a_map.m_allocator.allocator = nullptr;
			a_map.m_allocator.func = nullptr;
		}
		~OL_HashMap()
		{
			if (m_Hashes != nullptr)
			{
				//Call the destructor if it has one for the value.
				if constexpr (!trivalDestructableValue)
					for (size_t i = 0; i < m_capacity; i++)
						if (m_Hashes[i] != 0)
							m_Values[i].~Value();
				//Call the destructor if it has one for the key.
				if constexpr (!trivalDestructableKey)
					for (size_t i = 0; i < m_capacity; i++)
						if (m_Hashes[i] != 0)
							m_Keys[i].~Key();

				BBfree(m_allocator, m_Hashes);
			}
		}

		OL_HashMap<Key, Value>& operator=(const OL_HashMap<Key, Value>& a_rhs)
		{
			this->~OL_HashMap();

			m_capacity = a_rhs.m_capacity;
			m_size = 0;
			m_LoadCapacity = a_rhs.m_LoadCapacity;

			m_allocator = a_rhs.m_allocator;

			const size_t t_MemorySize = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_capacity;

			void* t_Buffer = BBalloc(m_allocator, t_MemorySize);
			m_Hashes = reinterpret_cast<Hash*>(t_Buffer);
			m_Keys = reinterpret_cast<Key*>(Pointer::Add(t_Buffer, sizeof(Hash) * m_capacity));
			m_Values = reinterpret_cast<Value*>(Pointer::Add(t_Buffer, (sizeof(Hash) + sizeof(Key)) * m_capacity));
			for (size_t i = 0; i < m_capacity; i++)
			{
				m_Hashes[i] = Hashmap_Specs::OL_EMPTY;
			}

			for (size_t i = 0; i < m_capacity; i++)
			{
				if (a_rhs.m_Hashes[i] != Hashmap_Specs::OL_EMPTY && a_rhs.m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE)
				{
					insert(a_rhs.m_Keys[i], a_rhs.m_Values[i]);
				}
			}

			return *this;
		}
		OL_HashMap<Key, Value>& operator=(OL_HashMap<Key, Value>&& a_rhs) noexcept
		{
			this->~OL_HashMap();

			m_capacity = a_rhs.m_capacity;
			m_size = a_rhs.m_size;
			m_LoadCapacity = a_rhs.m_LoadCapacity;

			m_allocator = a_rhs.m_allocator;

			m_Hashes = a_rhs.m_Hashes;
			m_Keys = a_rhs.m_Keys;
			m_Values = a_rhs.m_Values;

			a_rhs.m_capacity = 0;
			a_rhs.m_size = 0;
			a_rhs.m_LoadCapacity = 0;

			a_rhs.m_allocator.allocator = nullptr;
			a_rhs.m_allocator.func = nullptr;

			a_rhs.m_Hashes = nullptr;
			a_rhs.m_Keys = nullptr;
			a_rhs.m_Values = nullptr;

			return *this;
		}

		void insert(const Key& a_Key, Value& a_Res)
		{
			emplace(a_Key, a_Res);
		}
		template <class... Args>
		void emplace(const Key& a_Key, Args&&... a_ValueArgs)
		{
			if (m_size > m_LoadCapacity)
				grow();

			m_size++;
			const Hash t_Hash = Hash::MakeHash(a_Key) % m_capacity;


			for (size_t i = t_Hash; i < m_capacity; i++)
			{
				if (m_Hashes[i] == Hashmap_Specs::OL_EMPTY || m_Hashes[i] == Hashmap_Specs::OL_TOMBSTONE)
				{
					m_Hashes[i] = t_Hash;
					m_Keys[i] = a_Key;
					new (&m_Values[i]) Value(std::forward<Args>(a_ValueArgs)...);
					return;
				}
			}

			//Loop again but then from the start and stop at the hash. 
			for (size_t i = 0; i < t_Hash; i++)
			{
				if (m_Hashes[i] == Hashmap_Specs::OL_EMPTY || m_Hashes[i] == Hashmap_Specs::OL_TOMBSTONE)
				{
					m_Hashes[i] = t_Hash;
					m_Keys[i] = a_Key;
					new (&m_Values[i]) Value(std::forward<Args>(a_ValueArgs)...);
					return;
				}
			}
		}
		Value* find(const Key& a_Key) const
		{
			const Hash t_Hash = Hash::MakeHash(a_Key) % m_capacity;

			for (size_t i = t_Hash; i < m_capacity; i++)
			{
				if (m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE && KeyComp()(m_Keys[i], a_Key))
				{
					return &m_Values[i];
				}
				//If you hit an empty return a nullptr.
				if (m_Hashes[i] == Hashmap_Specs::OL_EMPTY)
				{
					return nullptr;
				}
			}

			//Loop again but then from the start and stop at the hash. 
			for (size_t i = 0; i < t_Hash; i++)
			{
				if (m_Hashes[i] != Hashmap_Specs::OL_TOMBSTONE && KeyComp()(m_Keys[i], a_Key))
				{
					return &m_Values[i];
				}
				//If you hit an empty return a nullptr.
				if (m_Hashes[i] == Hashmap_Specs::OL_EMPTY)
				{
					return nullptr;
				}
			}

			//Key does not exist.
			return nullptr;
		}
		void erase(const Key& a_Key)
		{
			const Hash t_Hash = Hash::MakeHash(a_Key) % m_capacity;

			for (size_t i = t_Hash; i < m_capacity; i++)
			{
				if (KeyComp()(m_Keys[i], a_Key))
				{
					m_Hashes[i] = Hashmap_Specs::OL_TOMBSTONE;
					//Call the destructor if it has one for the value.
					if constexpr (!trivalDestructableValue)
						m_Values[i].~Value();
					//Call the destructor if it has one for the key.
					if constexpr (!trivalDestructableKey)
						m_Keys[i].~Key();
					m_Keys[i] = 0;

					m_size--;
					return;
				}
			}

			//Loop again but then from the start and stop at the hash. 
			for (size_t i = 0; i < t_Hash; i++)
			{
				if (KeyComp()(m_Keys[i], a_Key))
				{
					m_Hashes[i] = Hashmap_Specs::OL_TOMBSTONE;
					//Call the destructor if it has one for the value.
					if constexpr (!trivalDestructableValue)
						m_Values[i].~Value();
					//Call the destructor if it has one for the key.
					if constexpr (!trivalDestructableKey)
						m_Keys[i].~Key();
					m_Keys[i] = 0;

					m_size--;
					return;
				}
			}
			BB_ASSERT(false, "OL_Hashmap remove called but key not found!");
		}
		void clear()
		{
			for (size_t i = 0; i < m_capacity; i++)
			{
				if (m_Hashes[i] != Hashmap_Specs::OL_EMPTY)
				{
					m_Hashes[i] = Hashmap_Specs::OL_EMPTY;
					if constexpr (!trivalDestructableValue)
						m_Values[i].~Value();
					if constexpr (!trivalDestructableKey)
						m_Keys[i].~Key();
					m_Keys[i] = 0;
				}
			}
			m_size = 0;
		}

		void reserve(const size_t a_size)
		{
			if (a_size > m_capacity)
			{
				size_t t_ModifiedCapacity = RoundUp(a_size, Hashmap_Specs::multipleValue);

				reallocate(t_ModifiedCapacity);
			}
		}

		size_t size() const { return m_size; }
	private:
		void grow(size_t a_MinCapacity = 1)
		{
			BB_WARNING(false, "Resizing an OL_HashMap, this might be a bit slow. Possibly reserve more.", WarningType::OPTIMALIZATION);

			size_t t_ModifiedCapacity = m_capacity * 2;

			if (a_MinCapacity > t_ModifiedCapacity)
				t_ModifiedCapacity = RoundUp(a_MinCapacity, Hashmap_Specs::multipleValue);

			reallocate(t_ModifiedCapacity);
		}
		void reallocate(const size_t a_NewLoadCapacity)
		{
			const size_t t_NewCapacity = LFCalculation(a_NewLoadCapacity, Hashmap_Specs::OL_LoadFactor);

			//Allocate the new buffer.
			const size_t t_MemorySize = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * t_NewCapacity;
			void* t_Buffer = BBalloc(m_allocator, t_MemorySize);

			Hash* t_NewHashes = reinterpret_cast<Hash*>(t_Buffer);
			Key* t_NewKeys = reinterpret_cast<Key*>(Pointer::Add(t_Buffer, sizeof(Hash) * t_NewCapacity));
			Value* t_NewValues = reinterpret_cast<Value*>(Pointer::Add(t_Buffer, (sizeof(Hash) + sizeof(Key)) * t_NewCapacity));
			for (size_t i = 0; i < t_NewCapacity; i++)
			{
				t_NewHashes[i] = Hashmap_Specs::OL_EMPTY;
			}

			for (size_t i = 0; i < m_capacity; i++)
			{
				if (m_Hashes[i] == i)
				{
					Key t_Key = m_Keys[i];
					Hash t_Hash = Hash::MakeHash(t_Key) % t_NewCapacity;

					while (t_NewHashes[t_Hash] != Hashmap_Specs::OL_EMPTY)
					{
						t_Hash++;
						if (t_Hash > t_NewCapacity)
							t_Hash = 0;
					}
					t_NewHashes[t_Hash] = t_Hash;
					t_NewKeys[t_Hash] = t_Key;
					t_NewValues[t_Hash] = m_Values[i];
				}
			}

			//Remove all the elements and free the memory.
			this->~OL_HashMap();

			m_Hashes = t_NewHashes;
			m_Keys = t_NewKeys;
			m_Values = t_NewValues;

			m_capacity = t_NewCapacity;
			m_LoadCapacity = a_NewLoadCapacity;
		}

	private:
		size_t m_capacity;
		size_t m_size;
		size_t m_LoadCapacity;

		//All the elements.
		Hash* m_Hashes;
		Key* m_Keys;
		Value* m_Values;

		Allocator m_allocator;
	};
}
#pragma endregion
