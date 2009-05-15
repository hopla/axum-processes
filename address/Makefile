CFLAGS=-Wall -Wextra -g -I`pg_config --includedir` -I../common
LFLAGS=-lmbn -L`pg_config --libdir` -lpq -lpthread
OBJECTS=db.o main.o ../common/common.a
HEADERS=db.h main.h
OUTPUT=axum-address

.PHONY: ../common/common.a

all: ${OUTPUT}

${OUTPUT}: ${OBJECTS}
	gcc ${OBJECTS} ${LFLAGS} -o ${OUTPUT}

%.o: %.c ${HEADERS} Makefile
	gcc ${CFLAGS} -c $*.c

../common/common.a:
	${MAKE} -C ../common all

clean:
	rm -rf ${OBJECTS} ${OUTPUT}
