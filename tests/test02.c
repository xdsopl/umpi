/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "test.h"

int main(int argc, char **argv)
{
	if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
		fprintf(stderr, "MPI initialization failed.\n");
		return 1;
	}
	int rounds = 10;
	if (argc == 2)
		rounds = atoi(argv[1]);
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	if (size < 2) {
		fprintf(stderr, "cant play this game alone.\n");
		return 1;
	}
	MPI_Status status;
	if (!rank)
		MPI_Send(0, 0, MPI_INT, 1, 0, MPI_COMM_WORLD);
	for (int i = rounds; i; i--) {
		MPI_Recv(0, 0, MPI_INT, (rank + size - 1) % size, 0, MPI_COMM_WORLD, &status);
		if (rank || (!rank && 1 < i))
			MPI_Send(0, 0, MPI_INT, (rank + 1) % size, 0, MPI_COMM_WORLD);
	}
	fprintf(stderr, "rank %d done playing.\n", rank);
	MPI_Finalize();
	return 0;
}
