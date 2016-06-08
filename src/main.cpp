#include <maxminddb.h>
#include <string>
#include "GarrysMod/Lua/Interface.h"

#define META_NAME "GeoIPDB"
#define META_ID 242

using namespace GarrysMod;

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

	MMDB_s *mmdb = new MMDB_s;

	std::string path = "garrysmod/" + std::string(LUA->GetString(1));

	int status = MMDB_open(path.c_str(), MMDB_MODE_MMAP, mmdb);

	if (status != MMDB_SUCCESS) {
		LUA->ThrowError("gm_geoip: Error opening database.");
	}

	Lua::UserData *userdata = (Lua::UserData *)LUA->NewUserdata(sizeof(Lua::UserData));
	userdata->data = mmdb;
	userdata->type = META_ID;

	LUA->CreateMetaTableType(META_NAME, META_ID);
	LUA->SetMetaTable(-2);

	return 1;
}

int GeoIPDB_GetIPInfo(lua_State *state) {
	LUA->CheckType(1, META_ID);
	LUA->CheckType(2, Lua::Type::STRING); // ip address

	Lua::UserData *ud = (Lua::UserData *)LUA->GetUserdata(1);
	MMDB_s *db = (MMDB_s *)ud->data;

	if (!db) {
		LUA->ThrowError("Attempt to call 'GetIPInfo' on bad GeoIPDB?");
		return 0;
	}

	int gai_error, mmdb_error;
	MMDB_lookup_result_s result = MMDB_lookup_string(db, LUA->GetString(2), &gai_error, &mmdb_error);

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

// todo: improve
int GeoIPDB_tostring(lua_State *state) {
	LUA->PushString("MMDB Instance");

	return 1;
}

int GeoIPDB_gc(lua_State *state) {
	Lua::UserData *ud = (Lua::UserData *)LUA->GetUserdata(1);
	MMDB_s *db = (MMDB_s *)ud->data;

	MMDB_close(db);
	delete db;

	ud->data = NULL;

	return 0;
}

GMOD_MODULE_OPEN() {
	LUA->PushSpecial(Lua::SPECIAL_GLOB);
		LUA->CreateTable();
			LUA->PushCFunction(geoip_OpenDB);
			LUA->SetField(-2, "OpenDB");
		LUA->SetField(-2, "geoip");
	LUA->Pop();

	LUA->CreateMetaTableType(META_NAME, META_ID);
		LUA->CreateTable();
			LUA->PushCFunction(GeoIPDB_GetIPInfo);
			LUA->SetField(-2, "GetIPInfo");
		LUA->SetField(-2, "__index");

		LUA->PushString("GeoIPDB");
		LUA->SetField(-2, "__type");

		LUA->PushCFunction(GeoIPDB_tostring);
		LUA->SetField(-2, "__tostring");

		LUA->PushCFunction(GeoIPDB_gc);
		LUA->SetField(-2, "__gc");
	LUA->Pop();

	return 0;
}

GMOD_MODULE_CLOSE() {
	// todo: close all open databases?

	return 0;
}
