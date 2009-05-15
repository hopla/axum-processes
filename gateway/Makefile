CFLAGS=-pedantic -Wall -Wextra -g
LFLAGS=-lmbn
OUTPUT=axum-gateway

all: ${OUTPUT}

${OUTPUT}: Makefile gateway.c
	gcc ${CFLAGS} gateway.c ${LFLAGS} -o ${OUTPUT}

clean:
	rm -f ${OUTPUT}
