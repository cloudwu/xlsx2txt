#include <windows.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>

static int
lShellExecute(lua_State *L) {
	const char * name = luaL_checkstring(L,1);
	HINSTANCE hinst = ShellExecuteA(NULL, "open", name, NULL, NULL, SW_SHOWNORMAL);
	lua_pushboolean(L, (unsigned)hinst > 32);
	return 1;
}

static int
lMessageBox(lua_State *L) {
	MessageBoxA(NULL, luaL_checkstring(L,1), "Error", MB_OK);
	return 0;
}

static int
lSleep(lua_State *L) {
	int ti = luaL_checkinteger(L, 1);
	Sleep(ti);
	return 0;
}

static int
lLastWriteTime(lua_State *L) {
	WIN32_FIND_DATAA ff32;
	const char *filename = luaL_checkstring(L,1);
	HANDLE hFind = FindFirstFileA(filename,&ff32);
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;
	uint64_t ti = ff32.ftLastWriteTime.dwLowDateTime |
		(uint64_t)ff32.ftLastWriteTime.dwHighDateTime << 32;

	// todo: for 5.3, use lua_Integer
	lua_pushnumber(L, (lua_Number)ti);
	FindClose(hFind);

	return 1;
}

static int
lCreateDirectory(lua_State *L) {
	BOOL err = CreateDirectoryA(luaL_checkstring(L,1), NULL);
	lua_pushboolean(L, err != 0);
	return 1;
}

static int
lRemoveDirectory(lua_State *L) {
	BOOL err =  RemoveDirectoryA(luaL_checkstring(L,1));
	lua_pushboolean(L, err != 0);
	return 1;
}

static int
lDeleteFile(lua_State *L) {
	BOOL err = DeleteFileA(luaL_checkstring(L, 1));
	lua_pushboolean(L, err != 0);
	return 1;
}

int
luaopen_winapi(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{"Sleep", lSleep},
		{"MessageBox", lMessageBox},
		{"ShellExecute" , lShellExecute},
		{"LastWriteTime", lLastWriteTime},
		{"CreateDirectory", lCreateDirectory},
		{"RemoveDirectory", lRemoveDirectory},
		{"DeleteFile", lDeleteFile},
		{NULL, NULL},
	};

	luaL_newlib(L,l);

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	return 1;
}
