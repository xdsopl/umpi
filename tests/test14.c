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
	if (rank == 0) {
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
		send._0 = 0;
		send._12[0] = 1;
		send._12[1] = 2;
		send._3 = 3;
		send._4 = 4;
		send._5 = 5;
		send._678[0] = 6;
		send._678[1] = 7;
		send._678[2] = 8;
		send._9 = 9;
		int count = 7;
		int array_of_blocklengths[] = { 1, 2, 1, 1, 1, 3, 1 };
		MPI_Aint array_of_displacements[] = {
			(char *)&send._0 - (char *)&send,
			(char *)send._12 - (char *)&send,
			(char *)&send._3 - (char *)&send,
			(char *)&send._4 - (char *)&send,
			(char *)&send._5 - (char *)&send,
			(char *)send._678 - (char *)&send,
			(char *)&send._9 - (char *)&send
		};
		MPI_Datatype array_of_types[] = { MPI_CHAR, MPI_FLOAT, MPI_INT, MPI_INT, MPI_SHORT, MPI_UNSIGNED, MPI_INT };
		MPI_Datatype tmp, type;
		MPI_Type_create_struct(count, array_of_blocklengths, array_of_displacements, array_of_types, &tmp);
		MPI_Type_create_resized(tmp, 0, sizeof(send), &type);
		MPI_Type_free(&tmp);
		MPI_Type_commit(&type);
		if (MPI_Send(&send, 1, type, 1, 0, MPI_COMM_WORLD)) {
			fprintf(stderr, "MPI_Send failed.\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
	}
	if (rank == 1) {
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
		} recv;
		int count = 8;
		int array_of_blocklengths[] = { 1, 2, 2, 1, 1, 1, 1, 1 };
		MPI_Aint array_of_displacements[] = {
			(char *)&recv._0 - (char *)&recv,
			(char *)recv._12 - (char *)&recv,
			(char *)recv._34 - (char *)&recv,
			(char *)&recv._5 - (char *)&recv,
			(char *)&recv._6 - (char *)&recv,
			(char *)&recv._7 - (char *)&recv,
			(char *)&recv._8 - (char *)&recv,
			(char *)&recv._9 - (char *)&recv
		};
		MPI_Datatype array_of_types[] = { MPI_CHAR, MPI_FLOAT, MPI_INT, MPI_SHORT, MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED, MPI_INT };
		MPI_Datatype tmp, type;
		MPI_Type_create_struct(count, array_of_blocklengths, array_of_displacements, array_of_types, &tmp);
		MPI_Type_create_resized(tmp, 0, sizeof(recv), &type);
		MPI_Type_free(&tmp);
		MPI_Type_commit(&type);
		if (MPI_Recv(&recv, 1, type, 0, 0, MPI_COMM_WORLD, 0)) {
			fprintf(stderr, "MPI_Recv failed.\n");
			MPI_Abort(MPI_COMM_WORLD, 1);
		}
		fprintf(stderr, "[ %d ] recieved: %d %g %g %d %d %d %d %d %d %d\n", rank, recv._0, recv._12[0], recv._12[1], recv._34[0], recv._34[1], recv._5, recv._6, recv._7, recv._8, recv._9);
	}
	MPI_Finalize();
	return 0;
}
