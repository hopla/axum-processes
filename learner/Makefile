include ../Makefile.inc
OUTPUT=axum-learner

all: ${OUTPUT}

${OUTPUT}: main.c
	gcc ${CFLAGS} main.c ${LFLAGS} -o ${OUTPUT}

clean:
	rm -f ${OUTPUT}
