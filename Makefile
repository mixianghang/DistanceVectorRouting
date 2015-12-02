CC=gcc
CFLAGS+=-pthread

mkDvServer: src/dv.c src/server.c
	$(CC) $(CFLAGS) src/dv.c src/server.c -o dvServer
