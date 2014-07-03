
CXXFLAGS = -std=c++11 -fno-exceptions -fno-rtti -D_GNU_SOURCE=1 -O3 -W -Wall -g
PREFIX = /opt/umpi

.PHONY: all test install clean

all: bin/mpicc bin/mpiexec

test: all
	$(MAKE) -C tests

lib/libmpi.so: umpi.cc *.hh include/mpi.h
	clang++ -stdlib=libc++ -o $@ $< $(CXXFLAGS) -shared -fPIC -lpthread

bin/mpicc: mpicc.cc lib/libmpi.so
	clang++ -stdlib=libc++ -o $@ $< $(CXXFLAGS)

bin/mpiexec: mpiexec.cc lib/libmpi.so
	clang++ -stdlib=libc++ -o $@ $< $(CXXFLAGS) -lpthread

install: all
	install -d $(PREFIX)/bin $(PREFIX)/lib $(PREFIX)/include
	install bin/mpicc bin/mpiexec $(PREFIX)/bin
	install lib/libmpi.so $(PREFIX)/lib
	install -m644 include/mpi.h $(PREFIX)/include
	ln -sf mpicc $(PREFIX)/bin/mpic++
	ln -sf mpicc $(PREFIX)/bin/mpiCC
	ln -sf mpicc $(PREFIX)/bin/mpicxx

clean:
	$(MAKE) -C tests clean
	rm -f lib/libmpi.so bin/mpiexec bin/mpicc

