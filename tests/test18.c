/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <stdio.h>
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
	int sendbuf[2] = { rank, 1 };
	fprintf(stderr, "[ %d ] my numbers are: %3d %3d\n", rank, sendbuf[0], sendbuf[1]);
	int recvbuf[2];
	if (MPI_Scan(sendbuf, recvbuf, 2, MPI_INT, MPI_SUM, MPI_COMM_WORLD)) {
		fprintf(stderr, "MPI_Scan failed\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	fprintf(stderr, "[ %d ] recieved sum Scan %3d %3d\n", rank, recvbuf[0], recvbuf[1]);
	MPI_Finalize();
	return 0;
}
