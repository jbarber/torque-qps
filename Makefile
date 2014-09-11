GITREV=$(shell git rev-parse --short --verify HEAD 2>/dev/null)
ifneq ($(strip ${GITREV}),)
VERSION:='-DVERSION="${GITREV}"'
else
VERSION:='-DVERSION="from unknown git revision"'
endif
LDLIBS=-ltorque
CXXFLAGS=--std=c++0x -I/usr/include/torque -g ${VERSION} -Wall
.PHONY=clean all

all: qps qps.1

qps: qps.C

clean:
	rm -rf qps qps.1 qps-*.tar.gz

qps.1: qps.1.pod
	pod2man $< > $@

TARVERSION=0.2.1
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
