
.PHONY: all address gateway engine clean

all: address gateway engine

address:
	${MAKE} -C address

gateway:
	${MAKE} -C gateway

engine:
	${MAKE} -C engine

clean:
	${MAKE} -C address clean
	${MAKE} -C gateway clean
	${MAKE} -C engine clean
	${MAKE} -C common clean

