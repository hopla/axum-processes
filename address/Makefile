CFLAGS=-Wall -Wextra -g `pkg-config --cflags json`
LFLAGS=-lmbn -lsqlite3 `pkg-config --libs json`
OBJECTS=conn.o db.o main.o
HEADERS=conn.h db.h
OUTPUT=axum-address

all: ${OUTPUT}

${OUTPUT}: ${OBJECTS}
	gcc ${OBJECTS} ${LFLAGS} -o ${OUTPUT}

%.o: %.c ${HEADERS} Makefile
	gcc ${CFLAGS} -c $*.c

clean:
	rm -rf ${OBJECTS} ${OUTPUT}
