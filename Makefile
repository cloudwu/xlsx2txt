ZLIB_DIR = luaclib/zlib/

ZLIB_SRC = $(addprefix $(ZLIB_DIR), \
 adler32.c\
 compress.c\
 crc32.c\
 deflate.c\
 gzclose.c\
 gzlib.c\
 gzread.c\
 gzwrite.c\
 infback.c\
 inffast.c\
 inflate.c\
 inftrees.c\
 trees.c\
 uncompr.c\
 zutil.c\
)

MINIZIP_DIR = luaclib/minizip/

MINIZIP_SRC = $(addprefix $(MINIZIP_DIR), \
ioapi.c\
iowin32.c\
mztools.c\
unzip.c\
zip.c\
)

CFLAGS = -O2 -Wall

all : xlsx2txt.exe

main.c : xlsx2txt.lua srlua.lua
	lua srlua.lua $< > $@

xlsx2txt.exe : main.c luaclib/luazip.c $(ZLIB_SRC) $(MINIZIP_SRC)
	gcc $(CFLAGS) -o $@ $^ -I$(ZLIB_DIR) -I$(MINIZIP_DIR) -I/usr/local/include -L/usr/local/lib -llua

clean :
	rm main.c xlsx2txt.exe

