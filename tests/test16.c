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
	double start = MPI_Wtime();
	fsleep(0.02 * rank);
	double stop = MPI_Wtime();
	int msec = 1000 * (stop - start);
	fprintf(stderr, "[ %d ] slept %d milliseconds.\n", rank, msec);
	char buf[size+1];
	memset(buf, 0, sizeof(buf));
	buf[rank] = '0' + rank%10;
	start = MPI_Wtime();
	if (MPI_Allgather(MPI_IN_PLACE, 0, 0, buf, 1, MPI_CHAR, MPI_COMM_WORLD)) {
		fprintf(stderr, "MPI_Allgather failed\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	stop = MPI_Wtime();
	msec = 1000 * (stop - start);
	fprintf(stderr, "[ %d ] received in place Allgather \"%s\" after %d milliseconds.\n", rank, buf, msec);
	MPI_Finalize();
	return 0;
}
