
.PHONY: all address gateway engine learner clean

all: address gateway engine learner

address:
	${MAKE} -C address

gateway:
	${MAKE} -C gateway

engine:
	${MAKE} -C engine

learner:
	${MAKE} -C learner

clean:
	${MAKE} -C address clean
	${MAKE} -C gateway clean
	${MAKE} -C engine clean
	${MAKE} -C learner clean
	${MAKE} -C common clean

