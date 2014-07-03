
CFLAGS = -std=c99 -D_GNU_SOURCE=1 -O3 -W -Wall -g

MPICC = ../bin/mpicc
MPIEXEC = ../bin/mpiexec

.PHONY: test clean

test: $(sort $(patsubst %.c,%-run,$(wildcard test??.c)))

.PRECIOUS: test%

test%: test%.c test.h $(MPICC)
	$(MPICC) -o $@ $< $(CFLAGS)

test%-run: test% $(MPIEXEC)
	$(MPIEXEC) -np 10 ./$<

clean:
	rm -f test[0-9][0-9]

