#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "unzip.h"
#include "zip.h"

struct unzipfile {
	unzFile f;
};

static unzFile
get_unzip(lua_State *L) {
	struct unzipfile * uf = lua_touserdata(L, 1);
	if (uf == NULL || uf->f == NULL)
		luaL_error(L, "unzip file %p is already closed", uf);
	return uf->f;
}

static int
lcloseunzip(lua_State *L) {
	struct unzipfile * uf = lua_touserdata(L, 1);
	if (uf == NULL || uf->f == NULL)
		return 0;
	unzClose(uf->f);
	uf->f = NULL;

	return 0;
}

static int
llistunzip(lua_State *L) {
	unzFile f = get_unzip(L);
	if (unzGoToFirstFile(f) != UNZ_OK) {
		return 0;
	}
	lua_newtable(L);
	int n = 0;
	do {
		++n;
		unz_file_info64 info;
		unzGetCurrentFileInfo64(f, &info, NULL, 0, NULL, 0, NULL, 0);
		uLong szfn = info.size_filename;
		char tmp[szfn];
		unzGetCurrentFileInfo64(f, NULL, tmp, szfn, NULL, 0, NULL, 0);
		lua_pushlstring(L, tmp, szfn);
		lua_rawseti(L, -2, n);
	} while(unzGoToNextFile(f) == UNZ_OK);

	return 1;
}

#define CHUNK_BYTE 0x10000

static int
lreadunzip(lua_State *L) {
	unzFile f = get_unzip(L);
	const char * filename = luaL_checkstring(L, 2);
	if (unzLocateFile(f, filename, 0) != UNZ_OK) {
		return luaL_error(L, "Can't find %s", filename);
	}
	unz_file_info64 info;
	unzGetCurrentFileInfo64(f, &info, NULL, 0, NULL, 0, NULL, 0);
	void *buf = lua_newuserdata(L, info.uncompressed_size);
	if (unzOpenCurrentFile(f) != UNZ_OK) {
		return luaL_error(L, "Can't open %s", filename);
	}

	size_t sz = info.uncompressed_size;
	char * ptr = buf;
	while (info.uncompressed_size) {
		int r = CHUNK_BYTE;
		if (r > info.uncompressed_size)
			r = info.uncompressed_size;
		int n = unzReadCurrentFile(f, ptr, r);
		if (n!=r) {
			unzCloseCurrentFile(f);
			return luaL_error(L, "Can't read %s", filename);
		}
		ptr += n;
		info.uncompressed_size -= n;
	}
	lua_pushlstring(L, buf, sz);
	unzCloseCurrentFile(f);

	return 1;
}

static int
lunzip(lua_State *L) {
	const char * filename = luaL_checkstring(L, 1);
	unzFile f = unzOpen64(filename);

	if (f == NULL)
		return 0;
	struct unzipfile * uf = lua_newuserdata(L, sizeof(*uf));
	uf->f = f;
	if (luaL_newmetatable(L, "unzip")) {
		lua_pushcfunction(L, lcloseunzip);
		lua_setfield(L, -2, "__gc");
		luaL_Reg l[] = {
			{ "list", llistunzip },
			{ "close", lcloseunzip },
			{ "read", lreadunzip },
			{ NULL, NULL },
		};
		luaL_newlib(L, l);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);

	return 1;
};


struct luazipfile {
	zipFile f;
};

static zipFile
get_zip(lua_State *L) {
	struct luazipfile * zf = lua_touserdata(L, 1);
	if (zf == NULL || zf->f == NULL)
		luaL_error(L, "zip file %p is already closed", zf);
	return zf->f;
}

static int
lclosezip(lua_State *L) {
	struct luazipfile * zf = lua_touserdata(L, 1);
	if (zf == NULL || zf->f == NULL)
		return 0;
	zipClose(zf->f, "");
	zf->f = NULL;

	return 0;
}

static int
lwritezip(lua_State *L) {
	zipFile f = get_zip(L);
	const char *filename = luaL_checkstring(L, 2);
	size_t sz = 0;
	const char *buffer = luaL_checklstring(L, 3, &sz);
	zip_fileinfo zfi;
	memset(&zfi,0, sizeof(zfi));
	if (zipOpenNewFileInZip64(f, filename, &zfi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION, 
		sz > 0xffffffff) != ZIP_OK) {
		return luaL_error(L, "Can't open %s", filename);
	}

	while (sz) {
		int r = CHUNK_BYTE;
		if (r > sz)
			r = sz;
		int n = zipWriteInFileInZip(f, buffer, r);
		if (n < 0) {
			return luaL_error(L, "Can't write %s", filename);
		}
		buffer += r;
		sz -= r;
	}

	if (zipCloseFileInZip(f) != ZIP_OK) {
		return luaL_error(L, "Can't close %s", filename);
	}

	return 0;
}

static int
lzip(lua_State *L) {
	const char * filename = luaL_checkstring(L, 1);
	zipFile f = zipOpen64(filename, APPEND_STATUS_CREATE);
	if (f == NULL)
		return 0;
	struct luazipfile * zf = lua_newuserdata(L, sizeof(*zf));
	zf->f = f;
	if (luaL_newmetatable(L, "zip")) {
		lua_pushcfunction(L, lclosezip);
		lua_setfield(L, -2, "__gc");
		luaL_Reg l[] = {
			{ "close", lclosezip },
			{ "write", lwritezip },
			{ NULL, NULL },
		};
		luaL_newlib(L, l);
		lua_setfield(L, -2, "__index");
	}
	lua_setmetatable(L, -2);
	return 1;
}

int
luaopen_zip(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{"unzip", lunzip},
		{"zip", lzip },
		{ NULL, NULL },
	};
	luaL_newlib(L,l);

	return 1;
}
