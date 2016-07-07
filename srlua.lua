local csource =[[
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdio.h>

int luaopen_zip(lua_State *L);

const char * lua_source = $SOURCE;

int
main(int argc, const char *argv[]) {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	luaL_requiref(L, "zip", luaopen_zip, 0);
	int err = luaL_loadstring(L, lua_source);
	if (err != LUA_OK) {
		fprintf(stderr, "%s", lua_tostring(L, -1));
		return 1;
	}

	int i;
	for (i=1;i<argc;i++) {
		lua_pushstring(L, argv[i]);
	}

	err = lua_pcall(L, argc-1, 0, 0);
	if (err != LUA_OK) {
		fprintf(stderr, "%s", lua_tostring(L, -1));
		return 1;
	}

	lua_close(L);

	return 0;
}
]]

local f = assert(io.open(...,"r"))
local tmp = {}
local escape = {
	['"'] = '\\"',
	["\\"] = "\\\\",
}
for line  in f:lines() do
	table.insert(tmp, string.format('"%s\\n"', line:gsub('["\\]',escape)))
end
local luasrc = table.concat(tmp, "\n")
f:close()

local tbl = { SOURCE = luasrc }

local fullsrc = csource:gsub("%$(%w+)", tbl)
print(fullsrc)

