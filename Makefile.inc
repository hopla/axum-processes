
# compile and link flags for all projects
CFLAGS=-Wall -Wextra -g -I`pg_config --includedir` -I../common
LFLAGS=-lmbn -L`pg_config --libdir` -lpq -lpthread ../common/common.a
HEADERS=../common/common.h


# make sure to compile the common library each time it changed
first: ../common/common.a ../common/common.h all

../common/common.a: ../common/common.o

../common/common.o: ../common/common.c ../common/common.h
	${MAKE} -C ../common all

