include ../Makefile.inc
OBJECTS+=db.o main.o
HEADERS+=db.h main.h
OUTPUT=axum-address

all: ${OUTPUT}

${OUTPUT}: ${OBJECTS}
	gcc ${OBJECTS} ${LFLAGS} -o ${OUTPUT}

%.o: %.c ${HEADERS}
	gcc ${CFLAGS} -c $*.c

clean:
	rm -rf ${OBJECTS} ${OUTPUT}
