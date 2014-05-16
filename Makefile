
CXXFLAGS = -std=c++11 -fno-exceptions -fno-rtti -D_GNU_SOURCE=1 -O3 -W -Wall -g

.PHONY: all test clean

all: bin/umpicc bin/umpiexec

test: all
	$(MAKE) -C tests

lib/libumpi.so: umpi.cc *.hh include/umpi.h
	clang++ -stdlib=libc++ -o $@ $< $(CXXFLAGS) -shared -fPIC -lpthread

bin/umpicc: umpicc.cc lib/libumpi.so
	clang++ -stdlib=libc++ -o $@ $< $(CXXFLAGS)

bin/umpiexec: umpiexec.cc lib/libumpi.so
	clang++ -stdlib=libc++ -o $@ $< $(CXXFLAGS) -lpthread

clean:
	$(MAKE) -C tests clean
	rm -f lib/libumpi.so bin/umpiexec bin/umpicc

