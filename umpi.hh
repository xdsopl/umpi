/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef UMPI_HH
#define UMPI_HH

#include "pool_allocator.hh"
#include "memory_pool.hh"
#include "based_ptr.hh"
#include "ipc.hh"

#include <list>
#include <forward_list>
#include <unistd.h>

static int UMPI_OUTSTANDING_REQUESTS_PER_PROCESS = 0;
static size_t UMPI_POOL_RESERVE_LEN = 1024 * 1024;
static const int UMPI_OUTSTANDING_COLLECTIVE_OPERATIONS = 16;
static const int UMPI_MAX_IN_PLACE_LEN = 64;
static const int UMPI_TAG_CC = -2;

enum umpi_id : ptrdiff_t {
	UMPI_ID_CONTIGUOUS,
	UMPI_ID_SCATTERED,
	UMPI_ID_PACKED,
	UMPI_ID_BYTE,
	UMPI_ID_CHAR,
	UMPI_ID_UNSIGNED_CHAR,
	UMPI_ID_SHORT,
	UMPI_ID_UNSIGNED_SHORT,
	UMPI_ID_INT,
	UMPI_ID_UNSIGNED,
	UMPI_ID_LONG,
	UMPI_ID_UNSIGNED_LONG,
	UMPI_ID_FLOAT,
	UMPI_ID_DOUBLE,
	UMPI_ID_LONG_DOUBLE
};

static void *mmap_addr = nullptr;
static size_t mmap_len = 0;

typedef struct umpi_request {} *MPI_Request;
typedef struct umpi_comm {} *MPI_Comm;

struct umpi_iovec {
	umpi_iovec() : dis_(0), len_(0) {}
	umpi_iovec(umpi_id id, size_t len) : dis_(id), len_(len) {}
	umpi_id id() const { return (umpi_id)dis_; }
	bool scattered() const { return id() == UMPI_ID_SCATTERED; }
	bool contiguous() const { return id() != UMPI_ID_SCATTERED; }
	size_t size() const { return scattered() ? len_ + 2 : 1; }
	size_t count() const { return scattered() ? len_ : 1; }
	ptrdiff_t stride() const { return contiguous() ? len_ : this[1].dis_; }
	size_t len() const { return contiguous() ? len_ : this[1].len_; }
	bool derived() const { return id() == UMPI_ID_CONTIGUOUS || id() == UMPI_ID_SCATTERED; }
	bool operator==(const umpi_iovec &other) const { return size() == other.size() && !memcmp(this, &other, sizeof(umpi_iovec) * size()); }
	struct umpi_iovec *begin() { return this + (scattered() ? 2 : 0); }
	struct umpi_iovec *end() { return this + size(); }
	ptrdiff_t dis_;
	size_t len_;
};
typedef based_ptr<struct umpi_iovec, &mmap_addr> iovec_pointer;
typedef struct umpi_datatype {
	umpi_datatype() : iovec_(nullptr) {}
	umpi_datatype(iovec_pointer iovec) : iovec_(iovec) {}
	~umpi_datatype()
	{
		if (!committed())
			delete &(*iovec_);
	}
	ptrdiff_t stride() const { return iovec_->stride(); }
	size_t len() const { return iovec_->len(); }
	umpi_id id() const { return iovec_->id(); }
	bool scattered() const { return iovec_->scattered(); }
	bool contiguous() const { return iovec_->contiguous(); }
	bool derived() const { return iovec_->derived(); }
	bool committed() const
	{
		void *begin = mmap_addr;
		void *end = static_cast<uint8_t *>(mmap_addr) + mmap_len;
		void *ptr = static_cast<void *>(iovec_);
		return begin <= ptr && ptr < end;
	}
	iovec_pointer iovec_;
} *MPI_Datatype;

typedef void MPI_User_function(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype);
typedef struct umpi_op *MPI_Op;

struct cookie : umpi_request {
	typedef based_ptr<class process, &mmap_addr> process_pointer;
	cookie(process_pointer owner, void *buf, iovec_pointer iovec, int size, int count, int source, int tag, int num = 1) :
		owner_(owner), buf_(buf), iovec_(iovec), size_(size), count_(count), tag_(tag), src_(source), err_(0), used_(1), busy_(num) {}
	cookie(const cookie &) = delete;
	cookie &operator=(const cookie &) = delete;
	void reset(process_pointer owner, void *buf, iovec_pointer iovec, int size, int count, int source, int tag, int num = 1)
	{
		owner_ = owner;
		buf_ = buf;
		iovec_ = iovec;
		size_ = size;
		count_ = count;
		tag_ = tag;
		src_ = source;
		err_ = 0;
		used_ = 1;
		busy_ = num;
	}
	bool recv_match(int source, int tag);
	bool send_match(int source, int tag);
	void wait();
	int write_to_owner(const void *buf, iovec_pointer iovec, int count, int seek = 0, int skip = 0);
	int read_from_owner(void *buf, iovec_pointer iovec, int size, int count = 0, int seek = 0, int skip = 0);
	// usually done within locked boxes, so no locking needed
	void join() { used_++; }
	void leave();
	bool owner() const;
	void *operator new(size_t size) = delete;
	void operator delete(void *ptr, size_t size) = delete;
	process_pointer owner_;
	void *buf_;
	iovec_pointer iovec_;
	int size_;
	int count_;
	int16_t tag_;
	int16_t src_;
	int16_t err_;
	int16_t used_;
	int16_t busy_;
};

typedef based_ptr<void, &mmap_addr> void_pointer;
typedef based_ptr<cookie, &mmap_addr> cookie_pointer;
typedef memory_pool<void_pointer, sizeof(cookie) + 2 * sizeof(cookie_pointer), ipc::mutex> void_pool;
static void_pool *pool_addr = nullptr;
typedef pool_allocator<void_pointer, void_pool, &pool_addr> void_allocator;
typedef std::list<cookie, typename void_allocator::template rebind<cookie>::other> box_type;

class process {
	enum waiting_for_type {
		not_waiting,
		waiting_for_incoming,
		waiting_for_any_cookie,
		waiting_for_cookie
	};
public:
	process() : pid_(getpid()), waiting_(nullptr), waiting_for_(not_waiting) {}
	void wait(cookie_pointer cookie);
	int wait_any(int count, umpi_request **cookies);
	cookie *wait_any(int source, int tag);
	void finalize(struct cookie *cookie);
	int send_request(MPI_Request *request, const void *buf, int count, MPI_Datatype datatype, int tag);
	int recv_request(MPI_Request *request, void *buf, int count, MPI_Datatype datatype, int source, int tag);
	void move_to_pending(box_type &box, box_type::iterator cookie);
	int notify_done(cookie_pointer cookie, int err);
	template <class... Args>
	cookie *create_request_locked(box_type &box, Args&&... args);
	const pid_t pid_;
private:
	template <class... Args>
	cookie *create_request(box_type &box, Args&&... args);
	ipc::mutex mutex_;
	ipc::condition_variable cond_;
	box_type free_;
	box_type inbox_;
	box_type outbox_;
	box_type pending_;
	cookie_pointer waiting_;
	waiting_for_type waiting_for_;
};

class collect {
	friend struct shared;
public:
	collect() : ticket_(0), joined_(0) {}
	bool joined_last();
	bool joined_first();
	int bcast_request(MPI_Request *request, void *buf, int count, MPI_Datatype datatype, int root);
	int root_bcast_request(MPI_Request *request, const void *buf, int count, MPI_Datatype datatype);
	int allgather_request(MPI_Request *request, const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype);
	int allreduce_request(MPI_Request *request, const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op);
	int gather_request(MPI_Request *request, const void *sendbuf, int sendcount, MPI_Datatype sendtype, int root);
	int root_gather_request(MPI_Request *request, void *recvbuf, int recvcount, MPI_Datatype recvtype);
	int scatter_request(MPI_Request *request, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root);
	int root_scatter_request(MPI_Request *request, const void *sendbuf, int sendcount, MPI_Datatype sendtype);
private:
	template <class... Args>
	cookie *create_request(box_type &box, Args&&... args);
	ipc::mutex mutex_;
	uint8_t sendbuf_[UMPI_MAX_IN_PLACE_LEN];
	uint8_t recvbuf_[UMPI_MAX_IN_PLACE_LEN];
	box_type box_;
	iovec_pointer iovec_;
	int count_;
	unsigned ticket_;
	int joined_;
};

struct shared {
	typedef based_ptr<process, &mmap_addr> process_pointer;
	typedef void_allocator::rebind<process>::other process_allocator;
	shared(int size, void *begin, void *end) :
		barrier(size), pool_state_(begin, end),
		iovec_packed_(UMPI_ID_PACKED, 1),
		iovec_byte_(UMPI_ID_BYTE, 1),
		iovec_char_(UMPI_ID_CHAR, sizeof(char)),
		iovec_unsigned_char_(UMPI_ID_UNSIGNED_CHAR, sizeof(unsigned char)),
		iovec_short_(UMPI_ID_SHORT, sizeof(short)),
		iovec_unsigned_short_(UMPI_ID_UNSIGNED_SHORT, sizeof(unsigned short)),
		iovec_int_(UMPI_ID_INT, sizeof(int)),
		iovec_unsigned_(UMPI_ID_UNSIGNED, sizeof(unsigned)),
		iovec_long_(UMPI_ID_LONG, sizeof(long)),
		iovec_unsigned_long_(UMPI_ID_UNSIGNED_LONG, sizeof(unsigned long)),
		iovec_float_(UMPI_ID_FLOAT, sizeof(float)),
		iovec_double_(UMPI_ID_DOUBLE, sizeof(double)),
		iovec_long_double_(UMPI_ID_LONG_DOUBLE, sizeof(long double))
	{
		pool_addr = &pool_state_;
		procs = process_allocator::allocate(size);
	}
	void init(int rank);
	void commit(iovec_pointer &iovec);
	collect *get_current_slot();
	ipc::barrier barrier;
	process_pointer procs;
private:
	ipc::mutex mutex_;
	void_pool pool_state_;
	collect slots_[UMPI_OUTSTANDING_COLLECTIVE_OPERATIONS];
	std::forward_list<iovec_pointer, void_allocator::rebind<iovec_pointer>::other> commited_;
	umpi_iovec iovec_packed_;
	umpi_iovec iovec_byte_;
	umpi_iovec iovec_char_;
	umpi_iovec iovec_unsigned_char_;
	umpi_iovec iovec_short_;
	umpi_iovec iovec_unsigned_short_;
	umpi_iovec iovec_int_;
	umpi_iovec iovec_unsigned_;
	umpi_iovec iovec_long_;
	umpi_iovec iovec_unsigned_long_;
	umpi_iovec iovec_float_;
	umpi_iovec iovec_double_;
	umpi_iovec iovec_long_double_;
};

static size_t calc_mmap_len(int size)
{
	if (!UMPI_OUTSTANDING_REQUESTS_PER_PROCESS)
		UMPI_OUTSTANDING_REQUESTS_PER_PROCESS = 2 * size;
	size_t page_size = sysconf(_SC_PAGE_SIZE);
	size_t header_len = sizeof(shared) + sizeof(process) * size;
	size_t cookie_len = sizeof(cookie) + 2 * sizeof(cookie_pointer);
	size_t pool_len = UMPI_POOL_RESERVE_LEN + cookie_len * UMPI_OUTSTANDING_REQUESTS_PER_PROCESS * size;
	return (header_len + pool_len + (page_size - 1)) & ~(page_size - 1);
}

#endif

