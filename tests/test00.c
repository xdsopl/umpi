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
	while (0 < count--) {
		MPI_Status status;
		char buffer[256];
		MPI_Recv(buffer, sizeof(buffer), MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		fprintf(stderr, "[ %d ] %s\n", status.MPI_SOURCE, buffer);
	}
}

void client(char *name, int rank)
{
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "This is client %d running on '%s'", rank, name);
	MPI_Send(buffer, sizeof(buffer), MPI_CHAR, 0, 0, MPI_COMM_WORLD);
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
