GITREV=$(shell git rev-parse --short --verify HEAD 2>/dev/null)
ifneq ($(strip ${GITREV}),)
VERSION:='-DVERSION="${GITREV}"' 
endif
LDFLAGS=-ltorque
CFLAGS=-std=c99 -g3 -O0 -I/usr/include/torque ${VERSION}
.PHONY=clean all

all: qps qps.1

qps: qps.c util.c

clean:
	rm -rf qps qps.1 qps-*.tar.gz

qps.1: qps.1.pod
	pod2man $< > $@

TARVERSION=0.0.1

TARBALL=qps-${TARVERSION}.tar.gz

${TARBALL}:
	git archive --format=tar.gz --prefix=qps-${TARVERSION}/ HEAD -o $@

rpm: ${TARBALL}
	rpmbuild -ta $<

srpm: ${TARBALL}
	rpmbuild -ts $<

install: qps qps.1
	install -m755 qps ${DESTDIR}/usr/bin
	install -m644 qps.1 ${DESTDIR}/usr/share/man/man1

test:
	true
