#include <maxminddb.h>
#include <string>
#include "GarrysMod/Lua/Interface.h"

using namespace GarrysMod;

MMDB_s mmdb;

bool grab_table_member(lua_State *state, const char *key, MMDB_entry_s *entry, const char *entry_key_1, const char *entry_key_2) {
	MMDB_entry_data_s entry_data;
	int status = MMDB_get_value(entry, &entry_data, entry_key_1, entry_key_2, NULL);

	if (status != MMDB_SUCCESS) {
		return false;
	}

	if (!entry_data.has_data) {
		return false;
	}

	switch (entry_data.type) {
		case MMDB_DATA_TYPE_UTF8_STRING:
			LUA->PushString(std::string(entry_data.utf8_string, entry_data.data_size).c_str());
			break;
		case MMDB_DATA_TYPE_DOUBLE:
			LUA->PushNumber(entry_data.double_value);
			break;
		case MMDB_DATA_TYPE_UINT16:
			LUA->PushNumber(entry_data.uint16);
			break;
		default:
			return false;
	}

	LUA->SetField(-2, key);

	return true;
}

// todo: database cache?
int geoip_OpenDB(lua_State *state) {
	LUA->CheckType(1, Lua::Type::STRING); // path relative to GarrysMod/garrysmod/

	const char* path = ("garrysmod/" + std::string(LUA->GetString(1))).c_str();

	int status = MMDB_open(path, MMDB_MODE_MMAP, &mmdb);

	LUA->PushBool(status == MMDB_SUCCESS);

	return 1;
}

int geoip_GetIPInfo(lua_State *state) {
	LUA->CheckType(1, Lua::Type::STRING); // ip address

	int gai_error, mmdb_error;
	MMDB_lookup_result_s result = MMDB_lookup_string(&mmdb, LUA->GetString(1), &gai_error, &mmdb_error);

	if (gai_error != 0) {
		LUA->PushBool(false);
		LUA->PushString(gai_strerror(gai_error));
		return 2;
	}

	if (mmdb_error != MMDB_SUCCESS) {
		LUA->PushBool(false);
		LUA->PushString(MMDB_strerror(mmdb_error));
		return 2;
	}

	if (!result.found_entry) {
		LUA->PushBool(false);
		LUA->PushString("No entry for this IP address was found");
		return 2;
	}

	LUA->CreateTable();

	grab_table_member(state, "country", &result.entry, "country", "iso_code");
	grab_table_member(state, "accuracy_radius", &result.entry, "location", "accuracy_radius");

	return 1;
}

GMOD_MODULE_OPEN() {
	LUA->PushSpecial(Lua::SPECIAL_GLOB);
		LUA->CreateTable();
			LUA->PushCFunction(geoip_OpenDB);
			LUA->SetField(-2, "OpenDB");

			LUA->PushCFunction(geoip_GetIPInfo);
			LUA->SetField(-2, "GetIPInfo");
		LUA->SetField(-2, "geoip");
	LUA->Pop();

	return 0;
}

GMOD_MODULE_CLOSE() {
	MMDB_close(&mmdb);

	return 0;
}
