CFLAGS=
LFLAGS=-lsqlite3
OBJ=axum_engine.o mambanet_stack_axum.o
HEADERS=axum_engine_functions.h ddpci2040.h mambanet_stack_axum.h
TARGET=axum-engine

all: ${TARGET}

${TARGET}: *.o Makefile
	g++ ${OBJ} ${LFLAGS} -o ${TARGET}

%.o: %.c ${HEADERS} Makefile
	g++ ${CFLAGS} -c $*.c
