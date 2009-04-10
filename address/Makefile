CFLAGS=-std=c89 -ansi -pedantic -D_POSIX_C_SOURCE=2 -Wall -Wextra -g
LFLAGS=-lmbn -lsqlite3
OBJECTS=conn.o db.o main.o
HEADERS=conn.h db.h
OUTPUT=axum-address

$OUTPUT: ${OBJECTS}
	gcc ${LFLAGS} ${OBJECTS} -o ${OUTPUT}

%.o: %.c ${HEADERS} Makefile
	gcc ${CFLAGS} -c $*.c

clean:
	rm -f ${OBJECTS} ${OUTPUT}

