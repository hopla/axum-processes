include ../Makefile.inc
#LFLAGS+=-lsqlite3
OBJECTS+=engine.o db.o dsp.o
HEADERS+=engine.h engine_functions.h ddpci2040.h db.h dsp.h
TARGET=axum-engine

all: ${TARGET}

${TARGET}: ${OBJECTS} Makefile
	g++ ${OBJECTS} ${LFLAGS} -o ${TARGET}

%.o: %.c ${HEADERS} Makefile
	g++ ${CFLAGS} -c $*.c

clean:
	rm -rf ${OBJECTS} ${TARGET}
