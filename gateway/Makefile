include ../Makefile.inc
OBJECTS+=gateway.o if_scan.o
HEADERS+=if_scan.h
TARGET=axum-gateway

all: ${TARGET}

${TARGET}: ${OBJECTS} Makefile
	gcc ${OBJECTS} ${LFLAGS} -o ${TARGET}

%.o: %.c ${HEADERS} Makefile
	gcc ${CFLAGS} -c $*.c

clean:
	rm -rf ${OBJECTS} ${TARGET}
