#include "BBjson.hpp"
#include "OS/Program.h"

#include "Utils/Utils.h"
#include <string>

using namespace BB;

enum class TOKEN_TYPE : uint32_t
{
	OUT_OF_TOKENS,
	CURLY_OPEN,
	CURLY_CLOSE,
	COLON,
	STRING,
	NUMBER,
	ARRAY_OPEN,
	ARRAY_CLOSE,
	COMMA,
	BOOLEAN,
	NULL_TYPE
};

//16 bytes
struct BB::Token
{
	TOKEN_TYPE type = TOKEN_TYPE::OUT_OF_TOKENS;
	uint32_t str_size = 0;
	char* str = nullptr;
};

static char ignoreWhiteSpace(JsonFile& a_JsonFile)
{
	char character = a_JsonFile.data[a_JsonFile.pos++];
	while ((character == ' ' || character == '\n' || character == '\r'))
	{
		BB_ASSERT(a_JsonFile.pos <= a_JsonFile.size, "out of tokens");
		character = a_JsonFile.data[a_JsonFile.pos++];
	}

	return character;
}

static Token GetToken(JsonFile& a_JsonFile)
{
	Token token;

	if (a_JsonFile.pos > a_JsonFile.size) //If we are at the end of the file, we will produce no more tokens.
	{
		token.type = TOKEN_TYPE::OUT_OF_TOKENS;
		return token;
	}

	char character = ignoreWhiteSpace(a_JsonFile);

	if (character == '"') //is string
	{
		//get string length
		size_t t_StrLen = 0;
		while (a_JsonFile.data[a_JsonFile.pos + t_StrLen] != '"')
			++t_StrLen;

		token.type = TOKEN_TYPE::STRING;
		token.str_size = static_cast<uint32_t>(t_StrLen);
		token.str = &a_JsonFile.data[a_JsonFile.pos];

		a_JsonFile.pos += static_cast<uint32_t>(t_StrLen) + 1; //includes the last "
	}
	else if (character == '-' || (character >= '0' && character <= '9')) //is number
	{
		//get string length which are numbers
		size_t t_StrLen = 1;
		char t_Num = a_JsonFile.data[a_JsonFile.pos + t_StrLen];
		while (t_Num == '-' || (t_Num >= '0' && t_Num <= '9') || t_Num == '.')
			t_Num = a_JsonFile.data[a_JsonFile.pos + t_StrLen++];

		token.type = TOKEN_TYPE::NUMBER;
		token.str_size = static_cast<uint32_t>(t_StrLen);
		token.str = &a_JsonFile.data[a_JsonFile.pos - 1];

		a_JsonFile.pos += static_cast<uint32_t>(t_StrLen) - 1; //includes the last
	}
	else if (character == 'f') {
		token.type = TOKEN_TYPE::BOOLEAN;
		token.str_size = 5;
		token.str = &a_JsonFile.data[a_JsonFile.pos - 1];
		//Do a janky check to see if False was actually correctly written.
		BB_WARNING(Memory::Compare("false", &a_JsonFile.data[a_JsonFile.pos - 1], 5) == 0,
			"JSON file tried to read a boolean that was set to True but it's not written as True!",
			WarningType::MEDIUM);
		a_JsonFile.pos += 4;
	}
	else if (character == 't') {
		token.type = TOKEN_TYPE::BOOLEAN;
		token.str_size = 4;
		token.str = &a_JsonFile.data[a_JsonFile.pos - 1];
		//Do a janky check to see if True was actually correctly written.
		BB_WARNING(Memory::Compare("true", &a_JsonFile.data[a_JsonFile.pos - 1], 4) == 0,
			"JSON file tried to read a boolean that was set to True but it's not written as True!",
			WarningType::MEDIUM);
		a_JsonFile.pos += 3;
	}
	else if (character == 'n') {
		token.type = TOKEN_TYPE::NULL_TYPE;
		BB_WARNING(Memory::Compare("null", &a_JsonFile.data[a_JsonFile.pos - 1], 4) == 0,
			"JSON file tried to read a boolean that was set to True but it's not written as True!",
			WarningType::MEDIUM);
		a_JsonFile.pos += 3;
	}
	else if (character == '{')
	{
		token.type = TOKEN_TYPE::CURLY_OPEN;
	}
	else if (character == '}')
	{
		token.type = TOKEN_TYPE::CURLY_CLOSE;
	}
	else if (character == '[')
	{
		token.type = TOKEN_TYPE::ARRAY_OPEN;
	}
	else if (character == ']')
	{
		token.type = TOKEN_TYPE::ARRAY_CLOSE;
	}
	else if (character == ':')
	{
		token.type = TOKEN_TYPE::COLON;
	}
	else if (character == ',')
	{
		token.type = TOKEN_TYPE::COMMA;
	}

	return token;
}

void BB::JsonNodeToString(const JsonNode* a_Node, String& a_string)
{
	switch (a_Node->type)
	{
	case BB::JSON_TYPE::OBJECT:
	{
		a_string.append("{");
		a_string.append("\n");

		const JsonObject::Pair* pair = a_Node->object->pairLL;
		while (pair != nullptr)
		{
			a_string.append("\"");
			a_string.append(pair->name);
			a_string.append("\"");
			a_string.append(" : ");
			JsonNodeToString(pair->node, a_string);

			if (pair->next != nullptr)
				a_string.append(",\n");
			else
				a_string.append("\n");
			pair = pair->next;
		}

		a_string.append("}\n");
	}
		break;
	case BB::JSON_TYPE::LIST:
		a_string.append("[\n");

		for (size_t i = 0; i < a_Node->list.node_count; i++)
		{
			JsonNodeToString(a_Node->list.nodes[i], a_string);
			a_string.append(",\n");
		}

		a_string.append("]\n");
		break;
	case BB::JSON_TYPE::STRING:
		a_string.append("\"");
		a_string.append(a_Node->string);
		a_string.append("\"");
		break;
	case BB::JSON_TYPE::NUMBER:
		BB_WARNING(false, "Not supporting number to string yet.", WarningType::LOW);
		a_string.append("\"");
		//t_JsonString.append(node->number);
		a_string.append("\"");
		break;
	case BB::JSON_TYPE::BOOL:
		if (a_Node->boolean)
			a_string.append("true");
		else
			a_string.append("false");
		break;
	case BB::JSON_TYPE::NULL_TYPE:
		a_string.append("null");
		break;
	}
}

JsonParser::JsonParser(const char* a_path)
{
	m_arena = MemoryArenaCreate();
	Buffer buffer = OSReadFile(m_arena, a_path);
	m_json_file.data = reinterpret_cast<char*>(buffer.data);
	m_json_file.size = static_cast<uint32_t>(buffer.size);
}

JsonParser::JsonParser(const Buffer& a_Buffer)
{
	m_arena = MemoryArenaCreate();
	m_json_file.data = reinterpret_cast<char*>(a_Buffer.data);
	m_json_file.size = static_cast<uint32_t>(a_Buffer.size);
}

JsonParser::~JsonParser()
{
	MemoryArenaFree(m_arena);
}

JsonNode* JsonParser::PraseSingleToken(const Token& a_token)
{
	JsonNode* node;
	switch (a_token.type)
	{
	case TOKEN_TYPE::CURLY_OPEN:
		node = ParseObject();
		break;
	case TOKEN_TYPE::ARRAY_OPEN:
		node = ParseList();
		break;
	case TOKEN_TYPE::STRING:
		node = ParseString(a_token);
		break;
	case TOKEN_TYPE::NUMBER:
		node = ParseNumber(a_token);
		break;
	case TOKEN_TYPE::BOOLEAN:
		node = ParseBoolean(a_token);
		break;
	default:
		BB_WARNING(false, "unknown json token found.", WarningType::HIGH);
		node = nullptr;
		break;
	}
	return node;
}

void JsonParser::Parse()
{
	Token token = GetToken(m_json_file);
	m_RootNode = PraseSingleToken(token);

	//ignore?
	/*token = GetToken(m_json_file);
	JsonNode* t_NextRoot = m_RootNode;
	while (token.type != TOKEN_TYPE::OUT_OF_TOKENS)
	{
		t_NextRoot->next = PraseSingleToken(token);
		token = GetToken(m_json_file);
		t_NextRoot = m_RootNode->next;
	}
	t_NextRoot->next = nullptr;*/
}

JsonNode* JsonParser::ParseObject()
{
	JsonNode* object_node = ArenaAllocType(m_arena, JsonNode);
	object_node->type = JSON_TYPE::OBJECT;

	JsonObject::Pair* pair_head = ArenaAllocType(m_arena, JsonObject::Pair);
	uint32_t pair_count = 0;

	Token next_token = GetToken(m_json_file);

	JsonObject::Pair* pair = pair_head;
	bool continue_loop = true;
	while (continue_loop)
	{
		BB_WARNING(next_token.type == TOKEN_TYPE::STRING, "Object does not start with a string!", WarningType::HIGH);
		char* element_name = ArenaAllocArr(m_arena, char, next_token.str_size + 1);
		Memory::Copy(element_name, next_token.str, next_token.str_size);
		element_name[next_token.str_size] = '\0';

		next_token = GetToken(m_json_file);
		BB_WARNING(next_token.type == TOKEN_TYPE::COLON, "token after string is not a :", WarningType::HIGH);
		next_token = GetToken(m_json_file); //the value

		pair->name = element_name;
		switch (next_token.type)
		{
		case TOKEN_TYPE::CURLY_OPEN:
			pair->node = ParseObject();
			break;
		case TOKEN_TYPE::ARRAY_OPEN:
			pair->node = ParseList();
			break;
		case TOKEN_TYPE::STRING:
			pair->node = ParseString(next_token);
			break;
		case TOKEN_TYPE::NUMBER:
			pair->node = ParseNumber(next_token);
			break;
		case TOKEN_TYPE::BOOLEAN:
			pair->node = ParseBoolean(next_token);
			break;
		case TOKEN_TYPE::NULL_TYPE:
			pair->node = ParseNull();
			break;
		case TOKEN_TYPE::OUT_OF_TOKENS:
			BB_ASSERT(false, "OUT_OF_TOKENS while reading a list in JSON");
			break;
		case TOKEN_TYPE::COLON:
		case TOKEN_TYPE::CURLY_CLOSE:
		case TOKEN_TYPE::ARRAY_CLOSE:
		case TOKEN_TYPE::COMMA:
			break;
		}

		next_token = GetToken(m_json_file);

		++pair_count;
		if (next_token.type == TOKEN_TYPE::CURLY_CLOSE)
			continue_loop = false;
		else
		{
			BB_ASSERT(next_token.type == TOKEN_TYPE::COMMA, "the token after a object pair should be a } or ,");
			next_token = GetToken(m_json_file);
			pair->next = ArenaAllocType(m_arena, JsonObject::Pair);
			pair = pair->next;
		}
	}
	object_node->object = ArenaAllocType(m_arena, JsonObject)(m_arena, pair_count, pair_head);
	pair = pair_head;
	for (size_t i = 0; i < pair_count; i++)
	{
		object_node->object->map.insert(pair->name, pair->node);
		pair = pair->next;
	}

	return object_node;
}

JsonNode* JsonParser::ParseList()
{
	JsonNode* node = ArenaAllocType(m_arena, JsonNode);
	node->type = JSON_TYPE::LIST;

	uint32_t list_size = 0;
	const uint32_t list_start_pos = static_cast<uint32_t>(m_json_file.pos);
	Token next_token = GetToken(m_json_file);
	while (next_token.type != TOKEN_TYPE::ARRAY_CLOSE) //get how many tokens we need to allocate
	{
		switch (next_token.type)
		{
		case TOKEN_TYPE::CURLY_OPEN:
		{
			uint32_t object_stack = 1;
			bool local_loop = true;
			next_token = GetToken(m_json_file);
			while (local_loop)
			{
				if (next_token.type == TOKEN_TYPE::CURLY_OPEN)
				{
					++object_stack;
				}
				else if (next_token.type == TOKEN_TYPE::CURLY_CLOSE)
				{
					if (--object_stack == 0)
						local_loop = false;
				}
				else
					next_token = GetToken(m_json_file);
			}
			++list_size;
		}
		break;
		case TOKEN_TYPE::ARRAY_OPEN:
		{
			uint32_t array_stack = 1;
			next_token = GetToken(m_json_file);
			bool local_loop = true;
			while (local_loop)
			{
				if (next_token.type == TOKEN_TYPE::ARRAY_OPEN)
				{
					++array_stack;
				}
				else if (next_token.type == TOKEN_TYPE::ARRAY_CLOSE)
				{
					if (--array_stack == 0)
						local_loop = false;
				}
				else
					next_token = GetToken(m_json_file);
			}
			++list_size;
		}
		break;
		case TOKEN_TYPE::STRING:
		case TOKEN_TYPE::NUMBER:
		case TOKEN_TYPE::BOOLEAN:
		case TOKEN_TYPE::NULL_TYPE:
			++list_size;
			break;
		case TOKEN_TYPE::OUT_OF_TOKENS:
			BB_ASSERT(false, "OUT_OF_TOKENS while reading a list in JSON");
			break;
		default: break;
		}

		next_token = GetToken(m_json_file);
	}
	//reset position back to the start of the list and iterate again
	m_json_file.pos = list_start_pos;

	node->list.node_count = list_size;
	node->list.nodes = ArenaAllocArr(m_arena, JsonNode*, node->list.node_count);

	next_token = GetToken(m_json_file);

	uint32_t node_index = 0;
	while(next_token.type != TOKEN_TYPE::ARRAY_CLOSE)
	{
		switch (next_token.type)
		{
		case TOKEN_TYPE::CURLY_OPEN:
			node->list.nodes[node_index++] = ParseObject();
			break;
		case TOKEN_TYPE::ARRAY_OPEN:
			node->list.nodes[node_index++] = ParseList();
			break;
		case TOKEN_TYPE::STRING:
			node->list.nodes[node_index++] = ParseString(next_token);
			break;
		case TOKEN_TYPE::NUMBER:
			node->list.nodes[node_index++] = ParseNumber(next_token);
			break;
		case TOKEN_TYPE::BOOLEAN:
			node->list.nodes[node_index++] = ParseBoolean(next_token);
			break;
		case TOKEN_TYPE::NULL_TYPE:
			node->list.nodes[node_index++] = ParseNull();
			break;
		case TOKEN_TYPE::CURLY_CLOSE:
			BB_ASSERT(false, "We should not reach this, algorithm mistake happened.");
			break;
		case TOKEN_TYPE::OUT_OF_TOKENS:
			BB_ASSERT(false, "OUT_OF_TOKENS while reading a list in JSON");
			break;
		default: break;
		}

		next_token = GetToken(m_json_file);
	}
	return node;
}

JsonNode* JsonParser::ParseString(const Token& a_token)
{
	JsonNode* node = ArenaAllocType(m_arena, JsonNode);
	node->type = JSON_TYPE::STRING;

	node->string = ArenaAllocArr(m_arena, char, a_token.str_size + 1);
	Memory::Copy(node->string, a_token.str, a_token.str_size);
	node->string[a_token.str_size] = '\0';

	return node;
}

JsonNode* JsonParser::ParseNumber(const Token& a_token)
{
	JsonNode* node = ArenaAllocType(m_arena, JsonNode);
	node->type = JSON_TYPE::NUMBER;

	node->number = std::stof(a_token.str);

	return node;
}

JsonNode* JsonParser::ParseBoolean(const Token& a_token)
{
	(void)a_token;
	JsonNode* node = ArenaAllocType(m_arena, JsonNode);
	node->type = JSON_TYPE::BOOL;

	const Token token = GetToken(m_json_file);
	if (strcmp(token.str, "False") == 0)
		node->boolean = false;
	else if (strcmp(token.str, "True") == 0)
		node->boolean = true;
	else
		BB_ASSERT(false, "JSON bool is wrongly typed, this assert should never be hit even with a wrong json anyway.");

	return node;
}

JsonNode* JsonParser::ParseNull()
{
	JsonNode* node = ArenaAllocType(m_arena, JsonNode);
	node->type = JSON_TYPE::NULL_TYPE;

	return node;
}
