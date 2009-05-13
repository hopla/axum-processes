CFLAGS=-Wall -Wextra -g -I`pg_config --includedir` `pkg-config --cflags json`
LFLAGS=-lmbn -L`pg_config --libdir` -lpq -lpthread `pkg-config --libs json`
OBJECTS=conn.o db.o main.o
HEADERS=conn.h db.h main.h
OUTPUT=axum-address

all: ${OUTPUT}

${OUTPUT}: ${OBJECTS}
	gcc ${OBJECTS} ${LFLAGS} -o ${OUTPUT}

%.o: %.c ${HEADERS} Makefile
	gcc ${CFLAGS} -c $*.c

clean:
	rm -rf ${OBJECTS} ${OUTPUT}
