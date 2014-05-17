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
	struct {
		float _a[3];
		int _34[2];
		char _0;
		unsigned _6;
		int _b;
		unsigned _7;
		int _9;
		short _5;
		unsigned _8;
		double _c;
		float _12[2];
		char _d;
	} recv[size];
	memset(recv, 0, sizeof(recv));
	MPI_Datatype tmp, recv_type, send_type;
	int recv_blocklengths[] = { 1, 2, 2, 1, 1, 1, 1, 1 };
	MPI_Aint recv_displacements[] = {
		(char *)&recv->_0 - (char *)recv,
		(char *)recv->_12 - (char *)recv,
		(char *)recv->_34 - (char *)recv,
		(char *)&recv->_5 - (char *)recv,
		(char *)&recv->_6 - (char *)recv,
		(char *)&recv->_7 - (char *)recv,
		(char *)&recv->_8 - (char *)recv,
		(char *)&recv->_9 - (char *)recv
	};
	MPI_Datatype recv_types[] = { MPI_CHAR, MPI_FLOAT, MPI_INT, MPI_SHORT, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED, MPI_INT };
	MPI_Type_create_struct(8, recv_blocklengths, recv_displacements, recv_types, &tmp);
	MPI_Type_create_resized(tmp, 0, (char *)(recv+1) - (char *)recv, &recv_type);
	MPI_Type_free(&tmp);
	MPI_Type_commit(&recv_type);
	struct {
		char _0;
		float _12[2];
		int _3;
		float _a;
		int _4;
		short _5;
		char _b[5];
		unsigned _678[3];
		int _9;
		long _c;
	} send;
	send._0 = rank + 0;
	send._12[0] = rank + 1;
	send._12[1] = rank + 2;
	send._3 = rank + 3;
	send._4 = rank + 4;
	send._5 = rank + 5;
	send._678[0] = rank + 6;
	send._678[1] = rank + 7;
	send._678[2] = rank + 8;
	send._9 = rank + 9;
	int send_blocklengths[] = { 1, 2, 1, 1, 1, 3, 1 };
	MPI_Aint send_displacements[] = {
		(char *)&send._0 - (char *)&send,
		(char *)send._12 - (char *)&send,
		(char *)&send._3 - (char *)&send,
		(char *)&send._4 - (char *)&send,
		(char *)&send._5 - (char *)&send,
		(char *)send._678 - (char *)&send,
		(char *)&send._9 - (char *)&send
	};
	MPI_Datatype send_types[] = { MPI_CHAR, MPI_FLOAT, MPI_INT, MPI_INT, MPI_SHORT, MPI_UNSIGNED, MPI_INT };
	MPI_Type_create_struct(7, send_blocklengths, send_displacements, send_types, &tmp);
	MPI_Type_create_resized(tmp, 0, sizeof(send), &send_type);
	MPI_Type_free(&tmp);
	MPI_Type_commit(&send_type);
	if (MPI_Allgather(&send, 1, send_type, recv, 1, recv_type, MPI_COMM_WORLD)) {
		fprintf(stderr, "MPI_Allgather failed.\n");
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	for (int j = 0; j < size; j++) {
		MPI_Barrier(MPI_COMM_WORLD);
		if (j == rank) {
			fprintf(stderr, "[ %d ] recieved:", rank);
			for (int i = 0; i < size; i++)
				fprintf(stderr, " (%d %g %g %d %d %d %d %d %d %d)", recv[i]._0, recv[i]._12[0], recv[i]._12[1], recv[i]._34[0], recv[i]._34[1], recv[i]._5, recv[i]._6, recv[i]._7, recv[i]._8, recv[i]._9);
			fprintf(stderr, "\n");
		}
	}
	MPI_Finalize();
	return 0;
}
