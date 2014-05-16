/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef MPI_H
#define MPI_H

#include <stddef.h>

#define MPI_MAX_PROCESSOR_NAME 256
#define MPI_SUCCESS 0
#define MPI_FAIL 1
#define MPI_ERR_IN_STATUS 2
#define MPI_ANY_SOURCE -1
#define MPI_ANY_TAG -1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct umpi_comm *MPI_Comm;
extern MPI_Comm MPI_COMM_WORLD;

typedef struct umpi_datatype *MPI_Datatype;
extern MPI_Datatype MPI_DATATYPE_NULL;
extern MPI_Datatype MPI_PACKED;
extern MPI_Datatype MPI_BYTE;
extern MPI_Datatype MPI_CHAR;
extern MPI_Datatype MPI_UNSIGNED_CHAR;
extern MPI_Datatype MPI_SHORT;
extern MPI_Datatype MPI_UNSIGNED_SHORT;
extern MPI_Datatype MPI_INT;
extern MPI_Datatype MPI_UNSIGNED;
extern MPI_Datatype MPI_LONG;
extern MPI_Datatype MPI_UNSIGNED_LONG;
extern MPI_Datatype MPI_FLOAT;
extern MPI_Datatype MPI_DOUBLE;
extern MPI_Datatype MPI_LONG_DOUBLE;

typedef struct umpi_op *MPI_Op;
extern MPI_Op MPI_MAX;
extern MPI_Op MPI_MIN;
extern MPI_Op MPI_SUM;
extern MPI_Op MPI_PROD;

typedef struct {
	int MPI_SOURCE;
	int MPI_TAG;
	int MPI_ERROR;
	size_t len;
} MPI_Status;

typedef struct umpi_request *MPI_Request;
typedef ptrdiff_t MPI_Aint;
typedef void MPI_User_function(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype);

double MPI_Wtime();
int MPI_Abort(MPI_Comm comm, int errorcode);
int MPI_Allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm);
int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
int MPI_Barrier(MPI_Comm comm);
int MPI_Bcast(void *buf, int count, MPI_Datatype datatype, int root, MPI_Comm comm);
int MPI_Comm_rank(MPI_Comm comm, int *rank);
int MPI_Comm_size(MPI_Comm comm, int *size);
int MPI_Finalize();
int MPI_Finalized(int *flag);
int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm);
int MPI_Get_address(const void *location, MPI_Aint *address);
int MPI_Get_count(const MPI_Status *status, MPI_Datatype datatype, int *count);
int MPI_Get_processor_name(char *name, int *resultlen);
int MPI_Iallgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request);
int MPI_Iallreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request);
int MPI_Igather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm, MPI_Request *request);
int MPI_Initialized(int *flag);
int MPI_Init(int *argc, char ***argv);
int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request);
int MPI_Ireduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, int root, MPI_Comm comm, MPI_Request *request);
int MPI_Iscatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm, MPI_Request *request);
int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);
int MPI_Issend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request);
int MPI_Op_create(MPI_User_function *user_fn, int commute, MPI_Op *op);
int MPI_Op_free(MPI_Op *op);
int MPI_Pack(const void *inbuf, int incount, MPI_Datatype datatype, void *outbuf, int outsize, int *position, MPI_Comm comm);
int MPI_Pack_size(int incount, MPI_Datatype datatype, MPI_Comm comm, int *size);
int MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status);
int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status);
int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm);
int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
int MPI_Ssend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
int MPI_Test(MPI_Request *request, int *flag, MPI_Status *status);
int MPI_Type_commit(MPI_Datatype *datatype);
int MPI_Type_contiguous(int count, MPI_Datatype oldtype, MPI_Datatype *newtype);
int MPI_Type_create_hindexed(int count, const int *array_of_blocklengths, const MPI_Aint *array_of_displacements, MPI_Datatype oldtype, MPI_Datatype *newtype);
int MPI_Type_create_resized(MPI_Datatype oldtype, MPI_Aint lb, MPI_Aint extent, MPI_Datatype *newtype);
int MPI_Type_create_struct(int count, const int *array_of_blocklengths, const MPI_Aint *array_of_displacements, const MPI_Datatype *array_of_types, MPI_Datatype *newtype);
int MPI_Type_free(MPI_Datatype *datatype);
int MPI_Unpack(const void *inbuf, int insize, int *position, void *outbuf, int outcount, MPI_Datatype datatype, MPI_Comm comm);
int MPI_Waitall(int count, MPI_Request *array_of_requests, MPI_Status *array_of_statuses);
int MPI_Wait(MPI_Request *request, MPI_Status *status);

#ifdef __cplusplus
}
#endif

#endif

