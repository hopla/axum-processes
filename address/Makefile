CFLAGS=-Wall -Wextra -g -Ijson/
LFLAGS=-lmbn -lsqlite3 json/.libs/libjson.a
OBJECTS=conn.o db.o main.o
HEADERS=conn.h db.h
OUTPUT=axum-address

all: ${OUTPUT}

${OUTPUT}: json/.libs/libjson.a ${OBJECTS}
	gcc ${OBJECTS} ${LFLAGS} -o ${OUTPUT}

%.o: %.c ${HEADERS} Makefile
	gcc ${CFLAGS} -c $*.c

clean:
	rm -rf ${OBJECTS} ${OUTPUT} json/

json/.libs/libjson.a: json/json-c-0.8.tar.gz
	cd json/ &&\
	  tar -xzf json-c-0.8.tar.gz &&\
	  mv json-c-0.8/* . &&\
	  ./configure &&\
	  make

json/json-c-0.8.tar.gz:
	mkdir json &&\
	  cd json &&\
	  wget http://oss.metaparadigm.com/json-c/json-c-0.8.tar.gz
