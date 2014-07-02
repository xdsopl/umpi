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
	static char msg[32 * 10];
	if (rank == 0) {
		MPI_Request request[10];
		for (int i = 0; i < 10; i++) {
			snprintf(msg + 32 * i, 10, "message %d", i);
			if (MPI_Isend(msg + 32 * i, strlen(msg + 32 * i) + 1, MPI_CHAR, 1, 0, MPI_COMM_WORLD, request + i)) {
				fprintf(stderr, "MPI_Isend failed\n");
				MPI_Abort(MPI_COMM_WORLD, 1);
			}
		}
		if (MPI_Waitall(10, request, MPI_STATUS_IGNORE)) {
			fprintf(stderr, "MPI_Waitall failed\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}

	}
	if (rank == 1) {
		for (int i = 0; i < 10; i++) {
			if (MPI_Recv(msg, sizeof(msg), MPI_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE)) {
				fprintf(stderr, "MPI_Recv failed\n");
				MPI_Abort(MPI_COMM_WORLD, 1);
			}
			fprintf(stderr, "[ %d ] recieved \"%s\".\n", rank, msg);
		}
	}
	MPI_Finalize();
	return 0;
}
