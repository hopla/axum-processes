include ../Makefile.inc
DEBUG=-g
CFLAGS+=$(DEBUG)
LFLAGS+=$(DEBUG) -rdynamic
OBJECTS+=engine.o db.o dsp.o backup.o
HEADERS+=engine.h engine_functions.h ddpci2040.h db.h dsp.h backup.h
TARGET=axum-engine

all: ${TARGET}

${TARGET}: ${OBJECTS} Makefile
	g++ ${OBJECTS} ${LFLAGS} -o ${TARGET}

%.o: %.c ${HEADERS} Makefile
	g++ ${CFLAGS} -c $*.c

clean:
	rm -rf ${OBJECTS} ${TARGET}
