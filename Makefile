LDFLAGS=-ltorque
CFLAGS=-std=c99 -g3 -O0
.PHONY=clean all

all: qps 

qps: qps.c

clean:
	rm -rf qps
