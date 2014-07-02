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
	char _0[1];
	int _12[2];
	float _345[3];
	double _6789[4];
	int sum = 0, len;
	MPI_Pack_size(1, MPI_CHAR, MPI_COMM_WORLD, &len);
	sum += len;
	MPI_Pack_size(2, MPI_INT, MPI_COMM_WORLD, &len);
	sum += len;
	MPI_Pack_size(3, MPI_FLOAT, MPI_COMM_WORLD, &len);
	sum += len;
	MPI_Pack_size(4, MPI_DOUBLE, MPI_COMM_WORLD, &len);
	sum += len;
	if (rank == 0) {
		memcpy(_0, (char[1]){ 0 }, sizeof(char[1]));
		memcpy(_12, (int[2]){ 1, 2 }, sizeof(int[2]));
		memcpy(_345, (float[3]){ 3.0f, 4.0f, 5.0f }, sizeof(float[3]));
		memcpy(_6789, (double[4]){ 6.0, 7.0, 8.0, 9.0 }, sizeof(double[4]));
		char sendbuf[sum];
		int pos = 0;
		int err = 0;
		err |= MPI_Pack(_0, 1, MPI_CHAR, sendbuf, sum, &pos, MPI_COMM_WORLD);
		err |= MPI_Pack(_12, 2, MPI_INT, sendbuf, sum, &pos, MPI_COMM_WORLD);
		err |= MPI_Pack(_345, 3, MPI_FLOAT, sendbuf, sum, &pos, MPI_COMM_WORLD);
		err |= MPI_Pack(_6789, 4, MPI_DOUBLE, sendbuf, sum, &pos, MPI_COMM_WORLD);
		if (err) {
			fprintf(stderr, "MPI_Pack failed.\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
		if (MPI_Send(sendbuf, pos, MPI_PACKED, 1, 0, MPI_COMM_WORLD)) {
			fprintf(stderr, "MPI_Send failed.\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
	}
	if (rank == 1) {
		char recvbuf[sum];
		if (MPI_Recv(recvbuf, sum, MPI_PACKED, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE)) {
			fprintf(stderr, "MPI_Recv failed.\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
		int pos = 0;
		int err = 0;
		err |= MPI_Unpack(recvbuf, sum, &pos, _0, 1, MPI_CHAR, MPI_COMM_WORLD);
		err |= MPI_Unpack(recvbuf, sum, &pos, _12, 2, MPI_INT, MPI_COMM_WORLD);
		err |= MPI_Unpack(recvbuf, sum, &pos, _345, 3, MPI_FLOAT, MPI_COMM_WORLD);
		err |= MPI_Unpack(recvbuf, sum, &pos, _6789, 4, MPI_DOUBLE, MPI_COMM_WORLD);
		if (err) {
			fprintf(stderr, "MPI_Unpack failed.\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
		fprintf(stderr, "[ %d ] recieved: %d %d %d %g %g %g %g %g %g %g\n", rank, _0[0], _12[0], _12[1], _345[0], _345[1], _345[2], _6789[0], _6789[1], _6789[2], _6789[3]);
	}
	MPI_Finalize();
	return 0;
}
