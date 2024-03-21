#pragma once
#include "Common.h"
#include "BBMemory.h"
#include "Storage/Hashmap.h"
#include "Storage/BBString.h"

//tutorial/guide used: https://kishoreganesh.com/post/writing-a-json-parser-in-cplusplus/
namespace BB
{
	enum class JSON_TYPE : uint32_t
	{
		OBJECT,
		LIST,
		STRING,
		NUMBER,
		BOOL,
		NULL_TYPE
	};

	struct JsonList //12 bytes
	{
		uint32_t node_count;
		struct JsonNode** nodes;
	};

	struct JsonNode //16 bytes
	{
		JSON_TYPE type = JSON_TYPE::NULL_TYPE; //4
		union //biggest type JsonList 12 bytes
		{
			struct JsonObject* object;
			JsonList list; //16
			char* string;
			float number;
			bool boolean;
		};

		inline struct JsonObject& GetObject() const
		{
			BB_ASSERT(type == JSON_TYPE::OBJECT, "json type not an object");
			return *object;
		}
		inline JsonList GetList() const
		{
			BB_ASSERT(type == JSON_TYPE::LIST, "json type not a list");
			return list;
		}
		inline char* GetString() const
		{
			BB_ASSERT(type == JSON_TYPE::STRING, "json type not a string");
			return string;
		}
		inline float GetNumber() const
		{
			BB_ASSERT(type == JSON_TYPE::NUMBER, "json type not a number");
			return number;
		}
		inline bool GetBoolean() const
		{
			BB_ASSERT(type == JSON_TYPE::BOOL, "json type not a bool");
			return boolean;
		}
	};

	struct JsonObject
	{
		struct Pair
		{
			char* name;
			JsonNode* node;
			Pair* next = nullptr;
		};

		JsonNode* Find(const char* a_str) const
		{
			JsonNode** node = map.find(a_str);
			BB_ASSERT(node, "node is nullptr, can't find node");
			return *node;
		}

		JsonObject(MemoryArena& a_arena, const uint32_t a_map_size, Pair* a_pair_head)
			: pairLL(a_pair_head)
		{
			map.Init(a_arena, a_map_size);
		}
	//private: this should be private
		StaticOL_HashMap<const char*, JsonNode*, String_KeyComp> map;
		Pair* pairLL;
	};

	struct JsonFile
	{
		char* data;
		uint32_t size;
		uint32_t pos = 0;
	};

	void JsonNodeToString(const JsonNode* node, String& a_string);
	struct Token;
	class JsonParser
	{
	public:
		//load from disk
		JsonParser(const char* a_path);
		//load from memory
		JsonParser(const Buffer& a_Buffer);
		~JsonParser();

		void Parse();

		JsonNode* GetRootNode() { return m_RootNode; }

		JsonNode* ParseObject();
		JsonNode* ParseList();
		JsonNode* ParseString(const Token& a_token);
		JsonNode* ParseNumber(const Token& a_token);
		JsonNode* ParseBoolean(const Token& a_token);
		JsonNode* ParseNull();
	private:
		
		//jank
		JsonNode* PraseSingleToken(const Token& a_token);
		MemoryArena m_arena;
		JsonFile m_json_file;

		JsonNode* m_RootNode = nullptr;
	};
}

