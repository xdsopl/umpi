/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "test.h"

// please also see test20 how to avoid the too generic and therefore expensive Allgatherv for simpler use cases

int main(int argc, char **argv)
{
	if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
		fprintf(stderr, "MPI initialization failed.\n");
		return 1;
	}
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	if (size < 2) {
		fprintf(stderr, "cant play this game alone.\n");
		return 1;
	}
	srand(rank + MPI_Wtime());
	int counts[size];
	for (int i = 0; i < size; i++)
		counts[i] = i % 7;
	int displs[size];
	displs[0] = 0;
	for (int i = 1; i < size; i++)
		displs[i] = displs[i - 1] + counts[i - 1];
	for (int j = 0; j < size; j++) {
		int i = rand() % (size - 1);
		if (displs[i] + counts[i] != displs[i+1])
			continue;
		displs[i] += counts[i+1];
		displs[i+1] -= counts[i];
	}
	int totalcount = 0;
	for (int i = 0; i < size; i++)
		totalcount += counts[i];
	char buf[totalcount+1];
	memset(buf, 0, sizeof(buf));
	for (int i = 0; i < counts[rank]; i++)
		buf[displs[rank] + i] = '0' + rank % 10;
	if (MPI_Allgatherv(MPI_IN_PLACE, 0, 0, buf, counts, displs, MPI_CHAR, MPI_COMM_WORLD)) {
		fprintf(stderr, "MPI_Allgatherv failed\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	fprintf(stderr, "[ %d ] recieved Allgatherv \"%s\"\n", rank, buf);
	MPI_Finalize();
	return 0;
}
