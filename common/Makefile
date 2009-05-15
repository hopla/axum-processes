CFLAGS=-Wall -Wextra -g
OBJECTS=common.o
HEADERS=common.h
TARGET=common.a

all: ${TARGET}

${TARGET}: ${OBJECTS}
	ar rcs ${TARGET} ${OBJECTS}

%.o: %.c ${HEADERS} Makefile
	gcc -c ${CFLAGS} $*.c

clean:
	rm -f ${TARGET} ${OBJECTS}

