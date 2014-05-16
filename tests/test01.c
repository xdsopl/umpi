/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include <stdio.h>
#include <mpi.h>
#include "test.h"

void server(char *name, int count)
{
	fprintf(stderr, "Server is running on '%s'.\n", name);
	MPI_Request request[count];
	MPI_Status status[count];
	char msg[256];
	snprintf(msg, sizeof(msg), "hello client");
	for (int i = 0; i < count; i++)
		MPI_Isend(msg, sizeof(msg), MPI_CHAR, i+1, 1, MPI_COMM_WORLD, request + i);
	if (MPI_Waitall(count, request, status)) {
		fprintf(stderr, "MPI_Waitall failed\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	char buffer[count * 256];
	for (int i = 0; i < count; i++)
		MPI_Irecv(buffer + 256 * i, 256, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, request + i);
	if (MPI_Waitall(count, request, status)) {
		fprintf(stderr, "MPI_Waitall failed\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	for (int i = 0; i < count; i++)
		fprintf(stderr, "[ %d ] %s\n", status[i].MPI_SOURCE, buffer + 256 * i);
}

void client(char *name, int rank)
{
	char buffer[256];
	MPI_Status status;
	if (MPI_Recv(buffer, sizeof(buffer), MPI_CHAR, 0, 1, MPI_COMM_WORLD, &status)) {
		fprintf(stderr, "MPI_Recv failed\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	fprintf(stderr, "[ %d ] %s\n", rank, buffer);
	snprintf(buffer, sizeof(buffer), "This is client %d running on '%s'", rank, name);
	if (MPI_Send(buffer, sizeof(buffer), MPI_CHAR, 0, 0, MPI_COMM_WORLD)) {
		fprintf(stderr, "MPI_Send failed\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
}

int main(int argc, char **argv)
{
	if (MPI_Init(&argc, &argv) != MPI_SUCCESS) {
		fprintf(stderr, "MPI initialization failed.\n");
		return 1;
	}
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	int len;
	char name[MPI_MAX_PROCESSOR_NAME];
	MPI_Get_processor_name(name, &len);
	if (size < 2)
		fprintf(stderr, "No client tasks available.\n");
	else if (rank == 0)
		server(name, size - 1);
	else
		client(name, rank);
	MPI_Finalize();
	return 0;
}
