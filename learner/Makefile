include ../Makefile.inc
OUTPUT=axum-learner

all: ${OUTPUT}

${OUTPUT}: main.c
	gcc ${CFLAGS} -D_XOPEN_SOURCE=500 main.c ${LFLAGS} -o ${OUTPUT}

clean:
	rm -f ${OUTPUT}
