CFLAGS=-pedantic -Wall -Wextra -g
LFLAGS=-lmbn
OUTPUT=axum-gateway

all: ${OUTPUT}

${OUTPUT}: Makefile
	gcc ${CFLAGS} gateway.c ${LFLAGS} -o ${OUTPUT}

