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
	srand(rank + MPI_Wtime());
	int sendcount = rand()%10 + 1;
	char sendbuf[sendcount];
	for (int i = 0; i < sendcount; i++)
		sendbuf[i] = '0' + rank%10;
	fprintf(stderr, "[ %d ] sendcount: %d\n", rank, sendcount);
	int root = size / 2;
	if (rank != root) {
		if (MPI_Gather(&sendcount, 1, MPI_INT, 0, 0, 0, root, MPI_COMM_WORLD)) {
			fprintf(stderr, "MPI_Gather failed\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
		if (MPI_Gatherv(sendbuf, sendcount, MPI_CHAR, 0, 0, 0, 0, root, MPI_COMM_WORLD)) {
			fprintf(stderr, "MPI_Gatherv failed\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
		MPI_Finalize();
		return 0;
	}
	int recvcounts[size];
	if (MPI_Gather(&sendcount, 1, MPI_INT, &recvcounts, 1, MPI_INT, root, MPI_COMM_WORLD)) {
		fprintf(stderr, "MPI_Gather failed\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	int totalcount = 0;
	for (int i = 0; i < size; i++)
		totalcount += recvcounts[i];
	fprintf(stderr, "[ %d ] totalcount: %d\n", rank, totalcount);
	char recvbuf[totalcount+1];
	memset(recvbuf, 0, sizeof(recvbuf));
	int displs[size];
	displs[0] = 0;
	for (int i = 1; i < size; i++)
		displs[i] = displs[i - 1] + recvcounts[i - 1];
	if (MPI_Gatherv(sendbuf, sendcount, MPI_CHAR, recvbuf, recvcounts, displs, MPI_CHAR, root, MPI_COMM_WORLD)) {
		fprintf(stderr, "MPI_Gatherv failed\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	fprintf(stderr, "[ %d ] recieved Gatherv \"%s\"\n", rank, recvbuf);
	MPI_Finalize();
	return 0;
}
