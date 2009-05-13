CFLAGS=-Wall -Wextra -g -I`pg_config --includedir`
LFLAGS=-lmbn -L`pg_config --libdir` -lpq -lpthread
OBJECTS=db.o main.o
HEADERS=db.h main.h
OUTPUT=axum-address

all: ${OUTPUT}

${OUTPUT}: ${OBJECTS}
	gcc ${OBJECTS} ${LFLAGS} -o ${OUTPUT}

%.o: %.c ${HEADERS} Makefile
	gcc ${CFLAGS} -c $*.c

clean:
	rm -rf ${OBJECTS} ${OUTPUT}
