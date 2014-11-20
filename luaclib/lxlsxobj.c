#include <lua.h>
#include <lauxlib.h>
#include <stdint.h>

#include <string.h>
#include <stdbool.h>

#define kEndOfChain 0xFFFFFFFEul

static inline uint16_t
Get16(const uint8_t *p) {
	return p[0] | p[1]<<8;
}

static inline uint32_t
Get32(const uint8_t *p) {
	return p[0] | p[1]<<8 | p[2]<<16 | p[3]<<24;
}

struct stream {
	const uint8_t * data;
	const uint8_t * ptr;
	const uint8_t * end;
};

static uint8_t *
patch_steam(lua_State *L, struct stream *s, uint8_t *tmp) {
	if (tmp)
		return tmp;
	tmp = lua_newuserdata(L, s->end - s->data);
	memcpy(tmp, s->data, s->end - s->data);
	return tmp;
}

static void
stream_init(struct stream *s, const uint8_t * ptr, size_t sz) {
	s->data = s->ptr = ptr;
	s->end = ptr + sz;
}

static const void *
ReadSector(struct stream *inStream,int sectorSizeBits, uint32_t sid) {
	const uint8_t *ptr = inStream->data + (((uint64_t)sid + 1) << sectorSizeBits);
	if (ptr + (1ul << sectorSizeBits) > inStream->end)
		return NULL;
	inStream->ptr = ptr + (1ul << sectorSizeBits);
	return ptr;
}

static const void *
ReadIDs(struct stream *inStream, int sectorSizeBits, uint32_t sid, uint32_t *dest) {
	const uint8_t * buf = ReadSector(inStream, sectorSizeBits, sid);
	if (buf == NULL)
		return NULL;
	uint32_t sectorSize = 1ul << sectorSizeBits;
	uint32_t t;
	for (t = 0; t < sectorSize; t += 4)
		*dest++ = Get32(buf + t);
	return buf;
}

static inline bool
is_zero(const uint8_t * tmp, int n) {
	int i;
	for (i=0;i<n;i++) {
		if (tmp[i])
			return false;
	}
	return true;
}

// The algorithm is from 7-zip , the Compound format
static int
lbin(lua_State *L) {
	size_t sz = 0;
	const uint8_t * p = (const uint8_t *)luaL_checklstring(L, 1, &sz);
	uint8_t header[] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
	if (memcmp(header, p, sizeof(header)) !=0)
		return 0;
	if (sz < 512)
		return 0;
	if (Get16(p + 0x1A) > 4) // majorVer
		return 0;
	if (Get16(p + 0x1C) != 0xFFFE)
		return 0;
	int sectorSizeBits = Get16(p + 0x1E);
	bool mode64bit = (sectorSizeBits >= 12);
	int miniSectorSizeBits = Get16(p + 0x20);
	if (sectorSizeBits > 28 || miniSectorSizeBits > 28 ||
		sectorSizeBits < 7 || miniSectorSizeBits < 2 || miniSectorSizeBits > sectorSizeBits)
		return 0;
	uint32_t numSectorsForFAT = Get32(p + 0x2C);
	uint32_t LongStreamMinSize = Get32(p + 0x38);
	uint32_t sectSize = 1ul << (int)sectorSizeBits;

	int ssb2 = (int)(sectorSizeBits - 2);
	uint32_t numSidsInSec = 1ul << ssb2;
	uint32_t numFatItems = numSectorsForFAT << ssb2;
	if ((numFatItems >> ssb2) != numSectorsForFAT)
		return 0;

	struct stream inStream;
	stream_init(&inStream, p, sz);
//Fat

	uint32_t Fat[numFatItems];
	memset(Fat, 0, sizeof(Fat));

	{
		uint32_t numSectorsForBat = Get32(p + 0x48);
		const uint32_t kNumHeaderBatItems = 109;
		uint32_t numBatItems = kNumHeaderBatItems + (numSectorsForBat << ssb2);
		if (numBatItems < kNumHeaderBatItems || ((numBatItems - kNumHeaderBatItems) >> ssb2) != numSectorsForBat)
			return 0;
		uint32_t bat[numBatItems];
		uint32_t i;
		for (i = 0; i < kNumHeaderBatItems; i++)
			bat[i] = Get32(p + 0x4c + i * 4);
		uint32_t sid = Get32(p + 0x44);
		uint32_t s;
		for (s = 0; s < numSectorsForBat; s++) {
			if (ReadIDs(&inStream, sectorSizeBits, sid, bat + i) == NULL)
				return 0;
			i += numSidsInSec - 1;
			sid = bat[i];
		}
		numBatItems = i;
		
		uint32_t j = 0;

		for (i = 0; i < numFatItems; j++, i += numSidsInSec) {
			if (j >= numBatItems)
				return 0;
			if (ReadIDs(&inStream, sectorSizeBits, bat[j], Fat + i) == NULL)
				return 0;
		}
	}

	uint8_t * copy_buf = NULL;
	{
		uint32_t sid = Get32(p + 0x30);
		for (;;) {
			if (sid >= numFatItems)
				return 0;
			const uint8_t * sect = ReadSector(&inStream, sectorSizeBits, sid);
			if (sect == NULL)
				return 0;
			uint32_t i;
			for (i = 0; i < sectSize; i += 128) {
				const uint8_t * p = (sect + i);
				// Check File time
				if (!is_zero(p+100,16)) {
					copy_buf = patch_steam(L, &inStream, copy_buf);
					memset(copy_buf + (p + 100 - inStream.data), 0, 16);
				}
			}
			sid = Fat[sid];
			if (sid == kEndOfChain)
				break;
		}
	}

	if (copy_buf) {
		lua_pushlstring(L, (const char *)copy_buf, sz);
		return 1;
	}
	return 0;
}

int
luaopen_xlsxobj(lua_State *L) {
	luaL_checkversion(L);
	luaL_Reg l[] = {
		{ "bin", lbin },
		{ NULL, NULL },
	};

	luaL_newlib(L,l);
	return 1;
}
