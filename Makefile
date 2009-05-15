
.PHONY: all address gateway engine clean

all: address gateway engine

address:
	${MAKE} -C address all

gateway:
	${MAKE} -C gateway all

engine:
	${MAKE} -C engine all

clean:
	${MAKE} -C address clean
	${MAKE} -C gateway clean
	${MAKE} -C engine clean
	${MAKE} -C common clean

