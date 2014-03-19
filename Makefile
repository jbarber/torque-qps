GITREV=$(shell git rev-parse --short --verify HEAD 2>/dev/null)
ifneq ($(strip ${GITREV}),)
VERSION:='-DVERSION="${GITREV}"' 
endif
LDFLAGS=-ltorque
CFLAGS=-std=c99 -g3 -O0 ${VERSION}
.PHONY=clean all

all: qps 

qps: qps.c util.c

clean:
	rm -rf qps
