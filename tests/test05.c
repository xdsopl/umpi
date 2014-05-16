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
	static char msg[32];
	if (rank == 0) {
		for (int i = 0; i < 10; i++) {
			snprintf(msg, sizeof(msg), "hello %d", i);
			if (MPI_Send(msg, strlen(msg) + 1, MPI_CHAR, 1, 0, MPI_COMM_WORLD)) {
				fprintf(stderr, "MPI_Send failed\n");
				MPI_Abort(MPI_COMM_WORLD, 1);
			}
			fsleep(0.01);
		}
	}
	if (rank == 1) {
		for (int i = 0; i < 10; i++) {
			MPI_Request request;
			if (MPI_Irecv(msg, sizeof(msg), MPI_CHAR, 0, 0, MPI_COMM_WORLD, &request)) {
				fprintf(stderr, "MPI_Irecv failed\n");
				MPI_Abort(MPI_COMM_WORLD, 1);
			}
			int counter = 0;
			int flag = 0;
			do {
				if (MPI_Test(&request, &flag, 0)) {
					fprintf(stderr, "MPI_Wait failed\n");
					MPI_Abort(MPI_COMM_WORLD, 1);
				}
				counter++;
			} while (!flag);
			fprintf(stderr, "[ %d ] did %d circles around rank 0.\n", rank, counter);
		}
	}
	MPI_Finalize();
	return 0;
}
