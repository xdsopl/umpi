
CXXFLAGS = -std=c++11 -fno-exceptions -fno-rtti -D_GNU_SOURCE=1 -O3 -W -Wall -g
PREFIX = /opt/umpi

.PHONY: all test install clean

all: bin/umpicc bin/umpiexec

test: all
	$(MAKE) -C tests

lib/libumpi.so: umpi.cc *.hh include/umpi.h
	clang++ -stdlib=libc++ -o $@ $< $(CXXFLAGS) -shared -fPIC -lpthread

bin/umpicc: umpicc.cc lib/libumpi.so
	clang++ -stdlib=libc++ -o $@ $< $(CXXFLAGS)

bin/umpiexec: umpiexec.cc lib/libumpi.so
	clang++ -stdlib=libc++ -o $@ $< $(CXXFLAGS) -lpthread

install: all
	install -d $(PREFIX)/bin $(PREFIX)/lib $(PREFIX)/include
	install bin/umpicc bin/umpiexec $(PREFIX)/bin
	install lib/libumpi.so $(PREFIX)/lib
	install -m644 include/umpi.h $(PREFIX)/include
	ln -sf umpi.h $(PREFIX)/include/mpi.h
	ln -sf umpicc $(PREFIX)/bin/mpic++
	ln -sf umpicc $(PREFIX)/bin/mpicc
	ln -sf umpicc $(PREFIX)/bin/mpiCC
	ln -sf umpicc $(PREFIX)/bin/mpicxx
	ln -sf umpicc $(PREFIX)/bin/umpic++
	ln -sf umpicc $(PREFIX)/bin/umpiCC
	ln -sf umpicc $(PREFIX)/bin/umpicxx
	ln -sf umpiexec $(PREFIX)/bin/mpiexec

clean:
	$(MAKE) -C tests clean
	rm -f lib/libumpi.so bin/umpiexec bin/umpicc

