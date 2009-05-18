include ../Makefile.inc
OUTPUT=axum-gateway

all: ${OUTPUT}

${OUTPUT}: gateway.c
	gcc ${CFLAGS} gateway.c ${LFLAGS} -o ${OUTPUT}

clean:
	rm -f ${OUTPUT}
