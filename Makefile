GITREV=$(shell git rev-parse --short --verify HEAD 2>/dev/null)
ifneq ($(strip ${GITREV}),)
VERSION:='-DVERSION="${GITREV}"'
else
VERSION:='-DVERSION="from unknown git revision"'
endif
LDLIBS=-ltorque
CXXFLAGS=--std=c++0x -I/usr/include/torque -g ${VERSION} -Wall
.PHONY=clean all test

all: qps qps.1

testsuite: LDLIBS+=-lgtest
testsuite: LDFLAGS+=-lgcov
testsuite: CXXFLAGS+=-DTESTING -fprofile-arcs -ftest-coverage
testsuite: util.C
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

tests: testsuite
	@./$^

util.C.gcov: util.C
	gcov -r ./$^

valgrind: testsuite
	valgrind --leak-check=full --suppressions=valgrind.sup ./$^

clean:
	rm -rf qps qps.1 qps-*.tar.gz testsuite *.o *.gcov *.gcda *.gcno

qps: qps.C util.C

qps.1: qps.1.pod
	pod2man $< > $@

TARVERSION=0.2.2
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
