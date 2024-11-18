#pragma once
#include "Utils/Hash.h"
#include "Utils/Utils.h"
#include "BBMemory.h"

#include "MemoryArena.hpp"

namespace BB
{
	namespace Hashmap_Specs
	{
		constexpr uint32_t Standard_Hashmap_Size = 64;

		constexpr const size_t multipleValue = 8;

		constexpr const float UM_LoadFactor = 1.f;
		constexpr const size_t UM_EMPTYNODE = 0xAABBCCDD;

		constexpr const float OL_LoadFactor = 1.3f;
		constexpr const float OL_UnLoadFactor = 0.7f;
		constexpr const size_t OL_TOMBSTONE = 0xDEADBEEFDEADBEEF;
		constexpr const size_t OL_EMPTY = 0xAABBCCDD;
	};

	//Calculate the load factor.
	static size_t LFCalculation(size_t a_size, float a_load_factor)
	{
		return static_cast<size_t>(static_cast<float>(a_size) * a_load_factor);
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
			HashEntry* next_entry = nullptr;
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
			m_load_capacity = a_size;
			m_entries = reinterpret_cast<HashEntry*>(BBalloc(m_allocator, m_capacity * sizeof(HashEntry)));

			for (size_t i = 0; i < m_capacity; i++)
			{
				new (&m_entries[i]) HashEntry();
			}
		}
		UM_HashMap(const UM_HashMap<Key, Value>& a_map)
		{
			m_allocator = a_map.m_allocator;
			m_size = a_map.m_size;
			m_capacity = a_map.m_capacity;
			m_load_capacity = a_map.m_load_capacity;

			m_entries = reinterpret_cast<HashEntry*>(BBalloc(m_allocator, m_capacity * sizeof(HashEntry)));

			//Copy over the hashmap and construct the element
			for (size_t i = 0; i < m_capacity; i++)
			{
				if (a_map.m_entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
				{
					new (&m_entries[i]) HashEntry(a_map.m_entries[i]);
					HashEntry* entry = &m_entries[i];
					HashEntry* previous_entry;
					while (entry->next_entry != nullptr)
					{
						previous_entry = entry;
						entry = reinterpret_cast<HashEntry*>(BBnew(m_allocator, HashEntry)(*entry->next_entry));
						previous_entry->next_entry = entry;
					}
				}
				else
				{
					new (&m_entries[i]) HashEntry();
				}
			}
		}
		UM_HashMap(UM_HashMap<Key, Value>&& a_map) noexcept
		{
			m_allocator = a_map.m_allocator;
			m_size = a_map.m_size;
			m_capacity = a_map.m_capacity;
			m_load_capacity = a_map.m_load_capacity;
			m_entries = a_map.m_entries;

			a_map.m_size = 0;
			a_map.m_capacity = 0;
			a_map.m_load_capacity = 0;
			a_map.m_entries = nullptr;
			a_map.m_allocator.allocator = nullptr;
			a_map.m_allocator.func = nullptr;
		}
		~UM_HashMap()
		{
			if (m_entries != nullptr)
			{
				clear();

				BBfree(m_allocator, m_entries);
			}
		}

		UM_HashMap<Key, Value>& operator=(const UM_HashMap<Key, Value>& a_rhs)
		{
			this->~UM_HashMap();

			m_allocator = a_rhs.m_allocator;
			m_size = a_rhs.m_size;
			m_capacity = a_rhs.m_capacity;
			m_load_capacity = a_rhs.m_load_capacity;

			m_entries = reinterpret_cast<HashEntry*>(BBalloc(m_allocator, m_capacity * sizeof(HashEntry)));

			//Copy over the hashmap and construct the element
			for (size_t i = 0; i < m_capacity; i++)
			{
				if (a_rhs.m_entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
				{
					new (&m_entries[i]) HashEntry(a_rhs.m_entries[i]);
					HashEntry* entry = &m_entries[i];
					HashEntry* previous_entry;
					while (entry->next_entry != nullptr)
					{
						previous_entry = entry;
						entry = reinterpret_cast<HashEntry*>(BBnew(m_allocator, HashEntry)(*entry->next_entry));
						previous_entry->next_entry = entry;
					}
				}
				else
				{
					new (&m_entries[i]) HashEntry();
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
			m_load_capacity = a_rhs.m_load_capacity;
			m_entries = a_rhs.m_entries;

			a_rhs.m_size = 0;
			a_rhs.m_capacity = 0;
			a_rhs.m_load_capacity = 0;
			a_rhs.m_entries = nullptr;
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
			if (m_size > m_load_capacity)
				grow();

			const Hash hash = Hash::MakeHash(a_key) % m_capacity;

			HashEntry* entry = &m_entries[hash.hash];
			if (entry->state == Hashmap_Specs::UM_EMPTYNODE)
			{
				entry->key = a_key;
				new (&entry->value) Value(std::forward<Args>(a_value_args)...);
				entry->next_entry = nullptr;
				return;
			}
			//Collision accurred, no problem we just create a linked list and make a new element.
			//Bad for cache memory though.
			while (entry)
			{
				if (entry->next_entry == nullptr)
				{
					HashEntry* new_entry = BBnew(m_allocator, HashEntry);
					new_entry->key = a_key;
					new (&new_entry->value) Value(std::forward<Args>(a_value_args)...);
					new_entry->next_entry = nullptr;
					entry->next_entry = new_entry;
					return;
				}
				entry = entry->next_entry;
			}
		}
		Value* find(const Key& a_key) const
		{
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;

			HashEntry* entry = &m_entries[hash];

			if (entry->state == Hashmap_Specs::UM_EMPTYNODE)
				return nullptr;

			while (entry)
			{
				if (Match(entry, a_key))
				{
					return &entry->value;
				}
				entry = entry->next_entry;
			}
			return nullptr;
		}
		void erase(const Key& a_key)
		{
			const Hash hash = Hash::MakeHash(a_key) % m_capacity;;

			HashEntry* entry = &m_entries[hash];
			if (Match(entry, a_key))
			{
				entry->~HashEntry();

				if (entry->next_entry != nullptr)
				{
					HashEntry* t_NextEntry = entry->next_entry;
					*entry = *entry->next_entry;
					BBfree(m_allocator, t_NextEntry);
					return;
				}

				entry->state = Hashmap_Specs::UM_EMPTYNODE;
				return;
			}

			HashEntry* previous_entry = nullptr;

			while (entry)
			{
				if (Match(entry, a_key))
				{
					previous_entry = entry->next_entry;
					BBfree(m_allocator, entry);
					return;
				}
				previous_entry = entry;
				entry = entry->next_entry;
			}
		}
		void clear()
		{
			//go through all the entries and individually delete the extra values from the linked list.
			//They need to be deleted seperatly since the memory is somewhere else.
			for (size_t i = 0; i < m_capacity; i++)
			{
				if (m_entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
				{
					HashEntry* t_NextEntry = m_entries[i].next_entry;
					while (t_NextEntry != nullptr)
					{
						HashEntry* t_DeleteEntry = t_NextEntry;
						t_NextEntry = t_NextEntry->next_entry;
						t_DeleteEntry->~HashEntry();

						BBfree(m_allocator, t_DeleteEntry);
					}
					m_entries[i].state = Hashmap_Specs::UM_EMPTYNODE;
				}
			}
			for (size_t i = 0; i < m_capacity; i++)
				if (m_entries[i].state == Hashmap_Specs::UM_EMPTYNODE)
					m_entries[i].~HashEntry();

			m_size = 0;
		}

		void reserve(const size_t a_size)
		{
			if (a_size > m_capacity)
			{
				size_t modified_capacity = RoundUp(a_size, Hashmap_Specs::multipleValue);

				reallocate(modified_capacity);
			}
		}

		size_t size() const { return m_size; }

	private:
		void grow(size_t a_MinCapacity = 1)
		{
			BB_WARNING(false, "Resizing an OL_HashMap, this might be a bit slow. Possibly reserve more.", WarningType::OPTIMALIZATION);

			size_t modified_capacity = m_capacity * 2;

			if (a_MinCapacity > modified_capacity)
				modified_capacity = RoundUp(a_MinCapacity, Hashmap_Specs::multipleValue);

			reallocate(modified_capacity);
		}

		void reallocate(const size_t a_NewLoadCapacity)
		{
			const size_t new_capacity = LFCalculation(a_NewLoadCapacity, Hashmap_Specs::UM_LoadFactor);

			//Allocate the new buffer.
			HashEntry* new_entries = reinterpret_cast<HashEntry*>(BBalloc(m_allocator, new_capacity * sizeof(HashEntry)));

			for (size_t i = 0; i < new_capacity; i++)
			{
				new (&new_entries[i]) HashEntry();
			}

			for (size_t i = 0; i < m_capacity; i++)
			{
				if (m_entries[i].state != Hashmap_Specs::UM_EMPTYNODE)
				{
					const Hash hash = Hash::MakeHash(m_entries[i].key) % new_capacity;

					HashEntry* entry = &new_entries[hash.hash];
					if (entry->state == Hashmap_Specs::UM_EMPTYNODE)
					{
						*entry = m_entries[i];
					}
					//Collision accurred, no problem we just create a linked list and make a new element.
					//Bad for cache memory though.
					while (entry)
					{
						if (entry->next_entry == nullptr)
						{
							HashEntry* new_entry = BBnew(m_allocator, HashEntry)(m_entries[i]);
						}
						entry = entry->next_entry;
					}
				}
			}

			this->~UM_HashMap();

			m_capacity = new_capacity;
			m_load_capacity = a_NewLoadCapacity;
			m_entries = new_entries;
		}

		size_t m_capacity;
		size_t m_load_capacity;
		size_t m_size = 0;

		HashEntry* m_entries;

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
			m_load_capacity = a_size;

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
			m_load_capacity = a_map.m_load_capacity;

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
			m_load_capacity = a_map.m_load_capacity;

			m_hashes = a_map.m_hashes;
			m_keys = a_map.m_keys;
			m_values = a_map.m_values;

			m_allocator = a_map.m_allocator;

			a_map.m_capacity = 0;
			a_map.m_size = 0;
			a_map.m_load_capacity = 0;
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
			m_load_capacity = a_rhs.m_load_capacity;

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
			m_load_capacity = a_rhs.m_load_capacity;

			m_allocator = a_rhs.m_allocator;

			m_hashes = a_rhs.m_hashes;
			m_keys = a_rhs.m_keys;
			m_values = a_rhs.m_values;

			a_rhs.m_capacity = 0;
			a_rhs.m_size = 0;
			a_rhs.m_load_capacity = 0;

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
			if (m_size > m_load_capacity)
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
				size_t modified_capacity = RoundUp(a_size, Hashmap_Specs::multipleValue);

				reallocate(modified_capacity);
			}
		}

		size_t size() const { return m_size; }
	private:
		void grow(size_t a_MinCapacity = 1)
		{
			BB_WARNING(false, "Resizing an OL_HashMap, this might be a bit slow. Possibly reserve more.", WarningType::OPTIMALIZATION);

			size_t modified_capacity = m_capacity * 2;

			if (a_MinCapacity > modified_capacity)
				modified_capacity = RoundUp(a_MinCapacity, Hashmap_Specs::multipleValue);

			reallocate(modified_capacity);
		}
		void reallocate(const size_t a_NewLoadCapacity)
		{
			const size_t new_capacity = LFCalculation(a_NewLoadCapacity, Hashmap_Specs::OL_LoadFactor);

			//Allocate the new buffer.
			const size_t memory_size = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * new_capacity;
			void* buffer = BBalloc(m_allocator, memory_size);

			Hash* new_hashes = reinterpret_cast<Hash*>(buffer);
			Key* new_keys = reinterpret_cast<Key*>(Pointer::Add(buffer, sizeof(Hash) * new_capacity));
			Value* new_values = reinterpret_cast<Value*>(Pointer::Add(buffer, (sizeof(Hash) + sizeof(Key)) * new_capacity));
			for (size_t i = 0; i < new_capacity; i++)
			{
				new_hashes[i] = Hashmap_Specs::OL_EMPTY;
			}

			for (size_t i = 0; i < m_capacity; i++)
			{
				if (m_hashes[i] == i)
				{
					Key t_Key = m_keys[i];
					Hash hash = Hash::MakeHash(t_Key) % new_capacity;

					while (new_hashes[hash] != Hashmap_Specs::OL_EMPTY)
					{
						hash++;
						if (hash > new_capacity)
							hash = 0;
					}
					new_hashes[hash] = hash;
					new_keys[hash] = t_Key;
					new_values[hash] = m_values[i];
				}
			}

			//Remove all the elements and free the memory.
			this->~OL_HashMap();

			m_hashes = new_hashes;
			m_keys = new_keys;
			m_values = new_values;

			m_capacity = new_capacity;
			m_load_capacity = a_NewLoadCapacity;
		}

	private:
		size_t m_capacity;
		size_t m_size;
		size_t m_load_capacity;

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
		StaticOL_HashMap() = default;

		void Init(Allocator a_allocator, const size_t a_size)
		{
			m_capacity = RoundUp(LFCalculation(a_size, Hashmap_Specs::OL_LoadFactor), 8);
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
		void Init(MemoryArena& a_arena, const size_t a_size)
		{
			m_capacity = RoundUp(LFCalculation(a_size, Hashmap_Specs::OL_LoadFactor), 8);
			m_size = 0;

			const size_t memory_size = (sizeof(Hash) + sizeof(Key) + sizeof(Value)) * m_capacity;

			void* buffer = ArenaAlloc(a_arena, memory_size, 8);
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

		void insert(const Key& a_key, const Value& a_res)
		{
			emplace(a_key, a_res);
		}
		template <class... Args>
		void emplace(const Key& a_key, Args&&... a_value_args)
		{
			m_size++;
			BB_WARNING(m_size < LFCalculation(m_capacity, Hashmap_Specs::OL_UnLoadFactor), "hashmap over loadfactor, collision slowdown will happen", WarningType::OPTIMALIZATION);
			BB_ASSERT(m_size < m_capacity, "OL_Hashmap out of capacity!");
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
		size_t m_size;
		size_t m_capacity;

		//All the elements.
		Hash* m_hashes;
		Key* m_keys;
		Value* m_values;
	};
#pragma region //Static Open Addressing Linear Probing (OL)
}
