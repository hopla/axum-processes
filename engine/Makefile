CFLAGS=-Wall -Wextra -I'pg_config --includedir'
LFLAGS=-lsqlite3 -L'pg_config --libdir' -lpq -lpthread
OBJECTS=axum_engine.o mambanet_stack_axum.o db.o
HEADERS=axum_engine_functions.h ddpci2040.h mambanet_stack_axum.h db.h
TARGET=axum-engine

all: ${TARGET}

${TARGET}: ${OBJECTS} Makefile
	g++ ${OBJECTS} ${LFLAGS} -o ${TARGET}

%.o: %.c ${HEADERS} Makefile
	g++ ${CFLAGS} -c $*.c

clean:
	rm -rf ${OBJECTS} ${TARGET}
