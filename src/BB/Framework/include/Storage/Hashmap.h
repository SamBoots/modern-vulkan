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

		void insert(const Key& a_key, Value& a_res)
		{
			emplace(a_key, a_res);
		}
		template <class... Args>
		void emplace(const Key& a_key, Args&&... a_value_args)
		{
			if (m_size > m_LoadCapacity)
				grow();

			const Hash hash = Hash::MakeHash(a_key) % m_capacity;

			HashEntry* t_Entry = &m_Entries[hash.hash];
			if (t_Entry->state == Hashmap_Specs::UM_EMPTYNODE)
			{
				t_Entry->key = a_key;
				new (&t_Entry->value) Value(std::forward<Args>(a_value_args)...);
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
					t_NewEntry->key = a_key;
					new (&t_NewEntry->value) Value(std::forward<Args>(a_value_args)...);
					t_NewEntry->next_Entry = nullptr;
					t_Entry->next_Entry = t_NewEntry;
					return;
				}
				t_Entry = t_Entry->next_Entry;
			}
		}
		Value* find(const Key& a_key) const
		{
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;

			HashEntry* t_Entry = &m_Entries[hash];

			if (t_Entry->state == Hashmap_Specs::UM_EMPTYNODE)
				return nullptr;

			while (t_Entry)
			{
				if (Match(t_Entry, a_key))
				{
					return &t_Entry->value;
				}
				t_Entry = t_Entry->next_Entry;
			}
			return nullptr;
		}
		void erase(const Key& a_key)
		{
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;;

			HashEntry* t_Entry = &m_Entries[hash];
			if (Match(t_Entry, a_key))
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
				if (Match(t_Entry, a_key))
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
					const Hash hash = Hash::MakeHash(m_Entries[i].key) % t_NewCapacity;

					HashEntry* t_Entry = &t_NewEntries[hash.hash];
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
		bool Match(const HashEntry* a_Entry, const Key& a_key) const
		{
			return KeyComp()(a_Entry->key, a_key);
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

			const size_t memory_size = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_capacity;

			void* buffer = BBalloc(m_allocator, memory_size);
			m_hashes = reinterpret_cast<Hash*>(buffer);
			m_keys = reinterpret_cast<Key*>(Pointer::Add(buffer, sizeof(Hash) * m_capacity));
			m_values = reinterpret_cast<Value*>(Pointer::Add(buffer, (sizeof(Hash) + sizeof(Key)) * m_capacity));
			for (size_t i = 0; i < m_capacity; i++)
			{
				m_hashes[i] = Hashmap_Specs::OL_EMPTY;
			}
		}
		OL_HashMap(const OL_HashMap<Key, Value>& a_map)
		{
			m_capacity = a_map.m_capacity;
			m_size = 0;
			m_LoadCapacity = a_map.m_LoadCapacity;

			m_allocator = a_map.m_allocator;

			const size_t memory_size = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_capacity;

			void* buffer = BBalloc(m_allocator, memory_size);
			m_hashes = reinterpret_cast<Hash*>(buffer);
			m_keys = reinterpret_cast<Key*>(Pointer::Add(buffer, sizeof(Hash) * m_capacity));
			m_values = reinterpret_cast<Value*>(Pointer::Add(buffer, (sizeof(Hash) + sizeof(Key)) * m_capacity));
			for (size_t i = 0; i < m_capacity; i++)
			{
				m_hashes[i] = Hashmap_Specs::OL_EMPTY;
			}

			for (size_t i = 0; i < m_capacity; i++)
			{
				if (a_map.m_hashes[i] != Hashmap_Specs::OL_EMPTY && a_map.m_hashes[i] != Hashmap_Specs::OL_TOMBSTONE)
				{
					insert(a_map.m_keys[i], a_map.m_values[i]);
				}
			}
		}
		OL_HashMap(OL_HashMap<Key, Value>&& a_map) noexcept
		{
			m_capacity = a_map.m_capacity;
			m_size = a_map.m_size;
			m_LoadCapacity = a_map.m_LoadCapacity;

			m_hashes = a_map.m_hashes;
			m_keys = a_map.m_keys;
			m_values = a_map.m_values;

			m_allocator = a_map.m_allocator;

			a_map.m_capacity = 0;
			a_map.m_size = 0;
			a_map.m_LoadCapacity = 0;
			a_map.m_hashes = nullptr;
			a_map.m_keys = nullptr;
			a_map.m_values = nullptr;

			a_map.m_allocator.allocator = nullptr;
			a_map.m_allocator.func = nullptr;
		}
		~OL_HashMap()
		{
			if (m_hashes != nullptr)
			{
				//Call the destructor if it has one for the value.
				if constexpr (!trivalDestructableValue)
					for (size_t i = 0; i < m_capacity; i++)
						if (m_hashes[i] != 0)
							m_values[i].~Value();
				//Call the destructor if it has one for the key.
				if constexpr (!trivalDestructableKey)
					for (size_t i = 0; i < m_capacity; i++)
						if (m_hashes[i] != 0)
							m_keys[i].~Key();

				BBfree(m_allocator, m_hashes);
			}
		}

		OL_HashMap<Key, Value>& operator=(const OL_HashMap<Key, Value>& a_rhs)
		{
			this->~OL_HashMap();

			m_capacity = a_rhs.m_capacity;
			m_size = 0;
			m_LoadCapacity = a_rhs.m_LoadCapacity;

			m_allocator = a_rhs.m_allocator;

			const size_t memory_size = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_capacity;

			void* buffer = BBalloc(m_allocator, memory_size);
			m_hashes = reinterpret_cast<Hash*>(buffer);
			m_keys = reinterpret_cast<Key*>(Pointer::Add(buffer, sizeof(Hash) * m_capacity));
			m_values = reinterpret_cast<Value*>(Pointer::Add(buffer, (sizeof(Hash) + sizeof(Key)) * m_capacity));
			for (size_t i = 0; i < m_capacity; i++)
			{
				m_hashes[i] = Hashmap_Specs::OL_EMPTY;
			}

			for (size_t i = 0; i < m_capacity; i++)
			{
				if (a_rhs.m_hashes[i] != Hashmap_Specs::OL_EMPTY && a_rhs.m_hashes[i] != Hashmap_Specs::OL_TOMBSTONE)
				{
					insert(a_rhs.m_keys[i], a_rhs.m_values[i]);
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

			m_hashes = a_rhs.m_hashes;
			m_keys = a_rhs.m_keys;
			m_values = a_rhs.m_values;

			a_rhs.m_capacity = 0;
			a_rhs.m_size = 0;
			a_rhs.m_LoadCapacity = 0;

			a_rhs.m_allocator.allocator = nullptr;
			a_rhs.m_allocator.func = nullptr;

			a_rhs.m_hashes = nullptr;
			a_rhs.m_keys = nullptr;
			a_rhs.m_values = nullptr;

			return *this;
		}

		void insert(const Key& a_key, Value& a_res)
		{
			emplace(a_key, a_res);
		}
		template <class... Args>
		void emplace(const Key& a_key, Args&&... a_value_args)
		{
			if (m_size > m_LoadCapacity)
				grow();

			m_size++;
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;


			for (size_t i = hash; i < m_capacity; i++)
			{
				if (m_hashes[i] == Hashmap_Specs::OL_EMPTY || m_hashes[i] == Hashmap_Specs::OL_TOMBSTONE)
				{
					m_hashes[i] = hash;
					m_keys[i] = a_key;
					new (&m_values[i]) Value(std::forward<Args>(a_value_args)...);
					return;
				}
			}

			//Loop again but then from the start and stop at the hash. 
			for (size_t i = 0; i < hash; i++)
			{
				if (m_hashes[i] == Hashmap_Specs::OL_EMPTY || m_hashes[i] == Hashmap_Specs::OL_TOMBSTONE)
				{
					m_hashes[i] = hash;
					m_keys[i] = a_key;
					new (&m_values[i]) Value(std::forward<Args>(a_value_args)...);
					return;
				}
			}
		}
		Value* find(const Key& a_key) const
		{
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;

			for (size_t i = hash; i < m_capacity; i++)
			{
				if (m_hashes[i] != Hashmap_Specs::OL_TOMBSTONE && KeyComp()(m_keys[i], a_key))
				{
					return &m_values[i];
				}
				//If you hit an empty return a nullptr.
				if (m_hashes[i] == Hashmap_Specs::OL_EMPTY)
				{
					return nullptr;
				}
			}

			//Loop again but then from the start and stop at the hash. 
			for (size_t i = 0; i < hash; i++)
			{
				if (m_hashes[i] != Hashmap_Specs::OL_TOMBSTONE && KeyComp()(m_keys[i], a_key))
				{
					return &m_values[i];
				}
				//If you hit an empty return a nullptr.
				if (m_hashes[i] == Hashmap_Specs::OL_EMPTY)
				{
					return nullptr;
				}
			}

			//Key does not exist.
			return nullptr;
		}
		void erase(const Key& a_key)
		{
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;

			for (size_t i = hash; i < m_capacity; i++)
			{
				if (KeyComp()(m_keys[i], a_key))
				{
					m_hashes[i] = Hashmap_Specs::OL_TOMBSTONE;
					//Call the destructor if it has one for the value.
					if constexpr (!trivalDestructableValue)
						m_values[i].~Value();
					//Call the destructor if it has one for the key.
					if constexpr (!trivalDestructableKey)
						m_keys[i].~Key();
					m_keys[i] = 0;

					m_size--;
					return;
				}
			}

			//Loop again but then from the start and stop at the hash. 
			for (size_t i = 0; i < hash; i++)
			{
				if (KeyComp()(m_keys[i], a_key))
				{
					m_hashes[i] = Hashmap_Specs::OL_TOMBSTONE;
					//Call the destructor if it has one for the value.
					if constexpr (!trivalDestructableValue)
						m_values[i].~Value();
					//Call the destructor if it has one for the key.
					if constexpr (!trivalDestructableKey)
						m_keys[i].~Key();
					m_keys[i] = 0;

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
				if (m_hashes[i] != Hashmap_Specs::OL_EMPTY)
				{
					m_hashes[i] = Hashmap_Specs::OL_EMPTY;
					if constexpr (!trivalDestructableValue)
						m_values[i].~Value();
					if constexpr (!trivalDestructableKey)
						m_keys[i].~Key();
					m_keys[i] = 0;
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
			const size_t memory_size = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * t_NewCapacity;
			void* buffer = BBalloc(m_allocator, memory_size);

			Hash* t_NewHashes = reinterpret_cast<Hash*>(buffer);
			Key* t_NewKeys = reinterpret_cast<Key*>(Pointer::Add(buffer, sizeof(Hash) * t_NewCapacity));
			Value* t_NewValues = reinterpret_cast<Value*>(Pointer::Add(buffer, (sizeof(Hash) + sizeof(Key)) * t_NewCapacity));
			for (size_t i = 0; i < t_NewCapacity; i++)
			{
				t_NewHashes[i] = Hashmap_Specs::OL_EMPTY;
			}

			for (size_t i = 0; i < m_capacity; i++)
			{
				if (m_hashes[i] == i)
				{
					Key t_Key = m_keys[i];
					Hash hash = Hash::MakeHash(t_Key) % t_NewCapacity;

					while (t_NewHashes[hash] != Hashmap_Specs::OL_EMPTY)
					{
						hash++;
						if (hash > t_NewCapacity)
							hash = 0;
					}
					t_NewHashes[hash] = hash;
					t_NewKeys[hash] = t_Key;
					t_NewValues[hash] = m_values[i];
				}
			}

			//Remove all the elements and free the memory.
			this->~OL_HashMap();

			m_hashes = t_NewHashes;
			m_keys = t_NewKeys;
			m_values = t_NewValues;

			m_capacity = t_NewCapacity;
			m_LoadCapacity = a_NewLoadCapacity;
		}

	private:
		size_t m_capacity;
		size_t m_size;
		size_t m_LoadCapacity;

		//All the elements.
		Hash* m_hashes;
		Key* m_keys;
		Value* m_values;

		Allocator m_allocator;
	};

#pragma endregion

#pragma region Static Open Addressing Linear Probing (OL)
	//Open addressing with Linear probing.
	template<typename Key, typename Value, typename KeyComp = Standard_KeyComp<Key>>
	class StaticOL_HashMap
	{
		static constexpr bool trivalDestructableValue = std::is_trivially_destructible_v<Value>;
		static constexpr bool trivalDestructableKey = std::is_trivially_destructible_v<Key>;

	public:
		StaticOL_HashMap()
		{
			m_hashes = nullptr;
			m_keys = nullptr;
			m_values = nullptr;

			m_capacity = 0;
			m_size = 0;
		}

		void Init(Allocator a_allocator, const size_t a_size)
		{
			m_capacity = a_size;
			m_size = 0;

			const size_t memory_size = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_capacity;

			void* buffer = BBalloc(a_allocator, memory_size);
			m_hashes = reinterpret_cast<Hash*>(buffer);
			m_keys = reinterpret_cast<Key*>(Pointer::Add(buffer, sizeof(Hash) * m_capacity));
			m_values = reinterpret_cast<Value*>(Pointer::Add(buffer, (sizeof(Hash) + sizeof(Key)) * m_capacity));
			for (size_t i = 0; i < m_capacity; i++)
			{
				m_hashes[i] = Hashmap_Specs::OL_EMPTY;
			}
		}
		void Destroy()
		{
			if (m_hashes != nullptr)
			{
				//Call the destructor if it has one for the value.
				if constexpr (!trivalDestructableValue)
					for (size_t i = 0; i < m_capacity; i++)
						if (m_hashes[i] != 0)
							m_values[i].~Value();
				//Call the destructor if it has one for the key.
				if constexpr (!trivalDestructableKey)
					for (size_t i = 0; i < m_capacity; i++)
						if (m_hashes[i] != 0)
							m_keys[i].~Key();
			}
		}

		StaticOL_HashMap(const StaticOL_HashMap<Key, Value>& a_map) = delete;
		StaticOL_HashMap(StaticOL_HashMap<Key, Value>&& a_map) = delete;
		StaticOL_HashMap<Key, Value>& operator=(const StaticOL_HashMap<Key, Value>& a_rhs) = delete;
		StaticOL_HashMap<Key, Value>& operator=(StaticOL_HashMap<Key, Value>&& a_rhs) = delete;

		void insert(const Key& a_key, Value& a_res)
		{
			emplace(a_key, a_res);
		}
		template <class... Args>
		void emplace(const Key& a_key, Args&&... a_value_args)
		{
			m_size++;
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;


			for (size_t i = hash; i < m_capacity; i++)
			{
				if (m_hashes[i] == Hashmap_Specs::OL_EMPTY || m_hashes[i] == Hashmap_Specs::OL_TOMBSTONE)
				{
					m_hashes[i] = hash;
					m_keys[i] = a_key;
					new (&m_values[i]) Value(std::forward<Args>(a_value_args)...);
					return;
				}
			}

			//Loop again but then from the start and stop at the hash. 
			for (size_t i = 0; i < hash; i++)
			{
				if (m_hashes[i] == Hashmap_Specs::OL_EMPTY || m_hashes[i] == Hashmap_Specs::OL_TOMBSTONE)
				{
					m_hashes[i] = hash;
					m_keys[i] = a_key;
					new (&m_values[i]) Value(std::forward<Args>(a_value_args)...);
					return;
				}
			}
		}
		Value* find(const Key& a_key) const
		{
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;

			for (size_t i = hash; i < m_capacity; i++)
			{
				if (m_hashes[i] != Hashmap_Specs::OL_TOMBSTONE && KeyComp()(m_keys[i], a_key))
				{
					return &m_values[i];
				}
				//If you hit an empty return a nullptr.
				if (m_hashes[i] == Hashmap_Specs::OL_EMPTY)
				{
					return nullptr;
				}
			}

			//Loop again but then from the start and stop at the hash. 
			for (size_t i = 0; i < hash; i++)
			{
				if (m_hashes[i] != Hashmap_Specs::OL_TOMBSTONE && KeyComp()(m_keys[i], a_key))
				{
					return &m_values[i];
				}
				//If you hit an empty return a nullptr.
				if (m_hashes[i] == Hashmap_Specs::OL_EMPTY)
				{
					return nullptr;
				}
			}

			//Key does not exist.
			return nullptr;
		}
		void erase(const Key& a_key)
		{
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;

			for (size_t i = hash; i < m_capacity; i++)
			{
				if (KeyComp()(m_keys[i], a_key))
				{
					m_hashes[i] = Hashmap_Specs::OL_TOMBSTONE;
					//Call the destructor if it has one for the value.
					if constexpr (!trivalDestructableValue)
						m_values[i].~Value();
					//Call the destructor if it has one for the key.
					if constexpr (!trivalDestructableKey)
						m_keys[i].~Key();
					m_keys[i] = 0;

					m_size--;
					return;
				}
			}

			//Loop again but then from the start and stop at the hash. 
			for (size_t i = 0; i < hash; i++)
			{
				if (KeyComp()(m_keys[i], a_key))
				{
					m_hashes[i] = Hashmap_Specs::OL_TOMBSTONE;
					//Call the destructor if it has one for the value.
					if constexpr (!trivalDestructableValue)
						m_values[i].~Value();
					//Call the destructor if it has one for the key.
					if constexpr (!trivalDestructableKey)
						m_keys[i].~Key();
					m_keys[i] = 0;

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
				if (m_hashes[i] != Hashmap_Specs::OL_EMPTY)
				{
					m_hashes[i] = Hashmap_Specs::OL_EMPTY;
					if constexpr (!trivalDestructableValue)
						m_values[i].~Value();
					if constexpr (!trivalDestructableKey)
						m_keys[i].~Key();
					m_keys[i] = 0;
				}
			}
			m_size = 0;
		}

		size_t size() const { return m_size; }
	private:
		//no load capacity here, we don't resize.
		size_t m_size;
		size_t m_capacity;

		//All the elements.
		Hash* m_hashes;
		Key* m_keys;
		Value* m_values;
	};
#pragma region //Static Open Addressing Linear Probing (OL)
}
