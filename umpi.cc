/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "include/mpi.h"
#include "umpi.hh"
#include "funcs.hh"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <fstream>

int umpi_in_place;
void *const MPI_IN_PLACE = static_cast<void *const>(&umpi_in_place);

struct umpi_comm umpi_comm_world;
struct umpi_comm umpi_comm_self;
MPI_Comm MPI_COMM_NULL = nullptr;
MPI_Comm MPI_COMM_WORLD = &umpi_comm_world;
MPI_Comm MPI_COMM_SELF = &umpi_comm_self;

struct umpi_datatype umpi_datatype_packed;
struct umpi_datatype umpi_datatype_byte;
struct umpi_datatype umpi_datatype_char;
struct umpi_datatype umpi_datatype_unsigned_char;
struct umpi_datatype umpi_datatype_short;
struct umpi_datatype umpi_datatype_unsigned_short;
struct umpi_datatype umpi_datatype_int;
struct umpi_datatype umpi_datatype_unsigned;
struct umpi_datatype umpi_datatype_long;
struct umpi_datatype umpi_datatype_unsigned_long;
struct umpi_datatype umpi_datatype_float;
struct umpi_datatype umpi_datatype_double;
struct umpi_datatype umpi_datatype_long_double;
MPI_Datatype MPI_DATATYPE_NULL = nullptr;
MPI_Datatype MPI_PACKED = &umpi_datatype_packed;
MPI_Datatype MPI_BYTE = &umpi_datatype_byte;
MPI_Datatype MPI_CHAR = &umpi_datatype_char;
MPI_Datatype MPI_UNSIGNED_CHAR = &umpi_datatype_unsigned_char;
MPI_Datatype MPI_SHORT = &umpi_datatype_short;
MPI_Datatype MPI_UNSIGNED_SHORT = &umpi_datatype_unsigned_short;
MPI_Datatype MPI_INT = &umpi_datatype_int;
MPI_Datatype MPI_UNSIGNED = &umpi_datatype_unsigned;
MPI_Datatype MPI_LONG = &umpi_datatype_long;
MPI_Datatype MPI_UNSIGNED_LONG = &umpi_datatype_unsigned_long;
MPI_Datatype MPI_FLOAT = &umpi_datatype_float;
MPI_Datatype MPI_DOUBLE = &umpi_datatype_double;
MPI_Datatype MPI_LONG_DOUBLE = &umpi_datatype_long_double;

umpi_generic_op<umpi_max> umpi_op_max;
umpi_generic_op<umpi_min> umpi_op_min;
umpi_generic_op<std::plus> umpi_op_sum;
umpi_generic_op<std::multiplies> umpi_op_prod;
MPI_Op MPI_MAX = &umpi_op_max;
MPI_Op MPI_MIN = &umpi_op_min;
MPI_Op MPI_SUM = &umpi_op_sum;
MPI_Op MPI_PROD = &umpi_op_prod;

void shared::init(int rank)
{
	pool_addr = &pool_state_;
	new (&procs[rank]) process();
	umpi_datatype_packed.iovec_ = &iovec_packed_;
	umpi_datatype_byte.iovec_ = &iovec_byte_;
	umpi_datatype_char.iovec_ = &iovec_char_;
	umpi_datatype_unsigned_char.iovec_ = &iovec_unsigned_char_;
	umpi_datatype_short.iovec_ = &iovec_short_;
	umpi_datatype_unsigned_short.iovec_ = &iovec_unsigned_short_;
	umpi_datatype_int.iovec_ = &iovec_int_;
	umpi_datatype_unsigned.iovec_ = &iovec_unsigned_;
	umpi_datatype_long.iovec_ = &iovec_long_;
	umpi_datatype_unsigned_long.iovec_ = &iovec_unsigned_long_;
	umpi_datatype_float.iovec_ = &iovec_float_;
	umpi_datatype_double.iovec_ = &iovec_double_;
	umpi_datatype_long_double.iovec_ = &iovec_long_double_;
}

static struct umpi {
	umpi(int s, int r, char *host) : shared(static_cast<struct shared *>(mmap_addr)), procs(shared->procs), self(procs + r), size(s), rank(r), ticket(0)
	{
		shared->init(rank);
		processor_name_len = snprintf(processor_name, MPI_MAX_PROCESSOR_NAME, "%s:%d", host, self->pid_);
		shared->barrier.wait();
	}
	~umpi()
	{
		shared->barrier.wait();
	}
	struct shared *shared;
	class process *procs;
	class process *self;
	char processor_name[MPI_MAX_PROCESSOR_NAME];
	int processor_name_len;
	int size;
	int rank;
	unsigned ticket;
} *umpi;

int MPI_Init(int *argc, char ***argv)
{
	(void)argc; (void)argv;

	if (umpi)
		return MPI_FAIL;

	char host[64];
	if (gethostname(host, sizeof(host))) {
		perror("gethostname");
		return MPI_FAIL;
	}
	int size = 1;
	int rank = 0;
	int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;
	int fd = -1;
	char *name = getenv("UMPI_MMAP");
	if (name) {
		size = atoi(getenv("UMPI_SIZE"));
		rank = atoi(getenv("UMPI_RANK"));
		mmap_flags = MAP_SHARED;
		fd = open(name, O_RDWR);
		if (fd == -1) {
			perror("open");
			return MPI_FAIL;
		}
	} else {
		std::cerr << "warning: missing UMPI environment. creating an artificial one!" << std::endl;
		std::ofstream("/proc/self/oom_score_adj") << 500 << std::endl;
	}

	mmap_len = calc_mmap_len(size);
	mmap_addr = mmap(0, mmap_len, PROT_READ|PROT_WRITE, mmap_flags, fd, 0);

	if (mmap_addr == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return MPI_FAIL;
	}
	if (fd != -1) {
		if (close(fd) == -1) {
			perror ("close");
			return MPI_FAIL;
		}
	} else {
		memset(mmap_addr, 0, mmap_len);
		new (mmap_addr) shared(size, static_cast<uint8_t *>(mmap_addr) + sizeof(shared), static_cast<uint8_t *>(mmap_addr) + mmap_len);
		umpi = new struct umpi(size, rank, host);
	}

	umpi = new struct umpi(size, rank, host);

	return MPI_SUCCESS;
}

int MPI_Initialized(int *flag)
{
	*flag = !!umpi;
	return MPI_SUCCESS;
}

int MPI_Finalized(int *flag)
{
	*flag = !umpi;
	return MPI_SUCCESS;
}

int MPI_Get_address(const void *location, MPI_Aint *address)
{
	*address = (MPI_Aint)location;
	return MPI_SUCCESS;
}

int MPI_Type_contiguous(int count, MPI_Datatype oldtype, MPI_Datatype *newtype)
{
	if (!umpi || !oldtype || !oldtype->iovec_ || !newtype)
		return MPI_FAIL;
	umpi_iovec *iovec;
	if (oldtype->contiguous()) {
		iovec = new umpi_iovec(UMPI_ID_CONTIGUOUS, count * oldtype->len());
	} else {
		size_t oldcount = oldtype->iovec_->count();
		size_t newcount = count * oldcount;
		iovec = new umpi_iovec[newcount + 2];
		iovec[0].dis_ = UMPI_ID_SCATTERED;
		iovec[0].len_ = newcount;
		iovec[1].dis_ = count * oldtype->stride();
		iovec[1].len_ = count * oldtype->len();
		for (size_t i = 0; i < newcount; i += oldcount)
			std::copy(oldtype->iovec_->begin(), oldtype->iovec_->end(), iovec->begin() + i);
	}
	*newtype = new umpi_datatype(iovec);
	return MPI_SUCCESS;
}

int MPI_Type_create_hindexed(int count, const int *array_of_blocklengths, const MPI_Aint *array_of_displacements, MPI_Datatype oldtype, MPI_Datatype *newtype)
{
	if (!umpi || !count || !array_of_blocklengths || !array_of_displacements || !oldtype || !oldtype->iovec_ || oldtype->scattered() || !newtype)
		return MPI_FAIL;
	for (int i = 0; i < count; i++)
		if (!array_of_blocklengths[i] || array_of_displacements[i] < 0)
			return MPI_FAIL;
	umpi_iovec *iovec = new umpi_iovec[count + 2];
	iovec[0].dis_ = UMPI_ID_SCATTERED;
	iovec[0].len_ = count;
	ptrdiff_t stride = 0;
	size_t sum = 0;
	const MPI_Aint *pdis = array_of_displacements;
	const int *plen = array_of_blocklengths;
	for (auto &e: *iovec) {
		ptrdiff_t dis = *pdis++;
		size_t len = *plen++ * oldtype->len();
		stride = std::max(ptrdiff_t(dis + len), stride);
		e.dis_ = dis;
		e.len_ = len;
		sum += len;
	}
	iovec[1].dis_ = stride;
	iovec[1].len_ = sum;
	*newtype = new umpi_datatype(iovec);
	return MPI_SUCCESS;
}

int MPI_Type_create_struct(int count, const int *array_of_blocklengths, const MPI_Aint *array_of_displacements, const MPI_Datatype *array_of_types, MPI_Datatype *newtype)
{
	if (!umpi || !count || !array_of_blocklengths || !array_of_displacements || !array_of_types || !newtype)
		return MPI_FAIL;
	for (int i = 0; i < count; i++)
		if (!array_of_blocklengths[i] || array_of_displacements[i] < 0 || array_of_types[i]->scattered())
			return MPI_FAIL;
	umpi_iovec *iovec = new umpi_iovec[count + 2];
	iovec[0].dis_ = UMPI_ID_SCATTERED;
	iovec[0].len_ = count;
	ptrdiff_t stride = 0;
	size_t sum = 0;
	const MPI_Aint *pdis = array_of_displacements;
	const int *plen = array_of_blocklengths;
	const MPI_Datatype *ptype = array_of_types;
	for (auto &e: *iovec) {
		ptrdiff_t dis = *pdis++;
		size_t len = *plen++ * (*ptype++)->len();
		stride = std::max(ptrdiff_t(dis + len), stride);
		e.dis_ = dis;
		e.len_ = len;
		sum += len;
	}
	iovec[1].dis_ = stride;
	iovec[1].len_ = sum;
	*newtype = new umpi_datatype(iovec);
	return MPI_SUCCESS;
}

int MPI_Type_create_resized(MPI_Datatype oldtype, MPI_Aint lb, MPI_Aint extent, MPI_Datatype *newtype)
{
	if (!umpi || !oldtype || !oldtype->iovec_ || lb || extent < oldtype->stride() || !newtype)
		return MPI_FAIL;
	umpi_iovec *iovec;
	if (oldtype->contiguous()) {
		iovec = new umpi_iovec[3];
		iovec[0].dis_ = UMPI_ID_SCATTERED;
		iovec[0].len_ = 1;
		iovec[1].dis_ = extent;
		iovec[1].len_ = oldtype->len();
		iovec[2].dis_ = 0;
		iovec[2].len_ = oldtype->len();
	} else {
		size_t size = oldtype->iovec_->size();
		iovec = new umpi_iovec[size];
		std::copy(oldtype->iovec_, oldtype->iovec_ + size, iovec);
		iovec[1].dis_ = extent;
	}
	*newtype = new umpi_datatype(iovec);
	return MPI_SUCCESS;
}

void shared::commit(iovec_pointer &iovec)
{
	mutex_.lock();
	for (auto &e: commited_) {
		if (*e == *iovec) {
			mutex_.unlock();
			delete &(*iovec);
			iovec = e;
			return;
		}
	}
	size_t size = iovec->size();
	iovec_pointer tmp = void_allocator::rebind<umpi_iovec>::other::allocate(size);
	std::copy(iovec, iovec + size, tmp);
	commited_.emplace_front(tmp);
	mutex_.unlock();
	delete &(*iovec);
	iovec = tmp;
}

int MPI_Type_commit(MPI_Datatype *datatype)
{
	if (!umpi || !datatype || !*datatype || !(*datatype)->iovec_)
		return MPI_FAIL;
	if (!(*datatype)->committed())
		umpi->shared->commit((*datatype)->iovec_);
	return MPI_SUCCESS;
}

int MPI_Type_free(MPI_Datatype *datatype)
{
	if (!umpi || !datatype || (*datatype && (!(*datatype)->iovec_ || !(*datatype)->derived())))
		return MPI_FAIL;
	delete *datatype;
	*datatype = nullptr;
	return MPI_SUCCESS;
}

double MPI_Wtime()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1000000000.0;
}

int MPI_Barrier(MPI_Comm comm)
{
	if (!umpi || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	umpi->shared->barrier.wait();
	return MPI_SUCCESS;
}

int MPI_Abort(MPI_Comm comm, int errorcode)
{
	if (comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	exit(errorcode);
	return MPI_SUCCESS;
}

int MPI_Finalize()
{
	if (!umpi)
		return MPI_FAIL;

	delete umpi;
	umpi = nullptr;

	if (munmap(mmap_addr, mmap_len)) {
		perror("munmap");
		return MPI_FAIL;
	}

	return MPI_SUCCESS;
}

int MPI_Op_create(MPI_User_function *user_fn, int commute, MPI_Op *op)
{
	if (!umpi || !user_fn || !commute)
		return MPI_FAIL;
	*op = new umpi_op(user_fn);
	return MPI_SUCCESS;
}

int MPI_Op_free(MPI_Op *op)
{
	delete *op;
	*op = nullptr;
	return MPI_SUCCESS;
}

int MPI_Get_processor_name(char *name, int *resultlen)
{
	if (!umpi || !name || !resultlen)
		return MPI_FAIL;
	*resultlen = umpi->processor_name_len;
	memcpy(name, umpi->processor_name, umpi->processor_name_len + 1);
	return MPI_SUCCESS;
}

int MPI_Comm_rank(MPI_Comm comm, int *rank)
{
	if (!umpi || !rank || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	*rank = umpi->rank;
	return MPI_SUCCESS;
}

int MPI_Comm_size(MPI_Comm comm, int *size)
{
	if (!umpi || !size || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	*size = umpi->size;
	return MPI_SUCCESS;
}

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newcomm)
{
	if (!comm)
		return MPI_ERR_COMM;
	if (!newcomm)
		return MPI_ERR_ARG;
	*newcomm = new umpi_comm(*comm);
	return *newcomm ? MPI_SUCCESS : MPI_ERR_NO_MEM;
}

int MPI_Comm_free(MPI_Comm *comm)
{
	if (!comm)
		return MPI_ERR_ARG;
	if (!*comm || *comm == MPI_COMM_SELF || *comm == MPI_COMM_WORLD)
		return MPI_ERR_COMM;
	delete *comm;
	return MPI_SUCCESS;
}

template <class... Args>
cookie *process::create_request(box_type &box, Args&&... args)
{
	if (free_.empty()) {
		box.emplace_back(umpi->self, std::forward<Args>(args)...);
	} else {
		box.splice(box.end(), free_, free_.begin());
		box.back().reset(umpi->self, std::forward<Args>(args)...);
	}
	return &box.back();
}

template <class... Args>
cookie *process::create_request_locked(box_type &box, Args&&... args)
{
	mutex_.lock();
	cookie *tmp = create_request(box, std::forward<Args>(args)...);
	mutex_.unlock();
	return tmp;
}

template <class... Args>
cookie *collect::create_request(box_type &box, Args&&... args)
{
	return umpi->self->create_request_locked(box, std::forward<Args>(args)...);
}

void process::finalize(struct cookie *cookie)
{
	mutex_.lock();
	cookie->used_--;
	for (auto cookie = pending_.begin(); cookie != pending_.end();) {
		if (!cookie->used_) {
			auto finished = cookie++;
			free_.splice(free_.begin(), pending_, finished);
		} else {
			++cookie;
		}
	}
	size_t trigger = UMPI_OUTSTANDING_REQUESTS_PER_PROCESS;
	size_t remaining = trigger / 2;
	if (free_.size() > trigger)
		free_.erase(std::next(free_.begin(), remaining), free_.end());
	mutex_.unlock();
}

int process::notify_done(cookie_pointer cookie, int err)
{
	mutex_.lock();
	cookie->err_ = cookie->err_ ? cookie->err_ : err;
	cookie->busy_--;
	if (!cookie->busy_ && waiting_for_ == waiting_for_cookie && waiting_ == cookie)
		cond_.notify_one();
	mutex_.unlock();
	return err;
}

size_t umpi_copy(const void *read_buf, iovec_pointer read_iovec, int read_count, int read_skip, void *write_buf, iovec_pointer write_iovec, int write_count, int write_seek)
{
	size_t total_bytes = read_count * read_iovec->len();
	if (total_bytes > write_count * write_iovec->len())
		return 0;
	if (read_iovec->contiguous() && write_iovec->contiguous()) {
		memcpy(
			static_cast<uint8_t *>(write_buf) + write_seek * write_iovec->len(),
			static_cast<const uint8_t *>(read_buf) + read_skip * read_iovec->len(),
			total_bytes
		);
		return total_bytes;
	}
	// TODO: do this without process_vm_writev or process_vm_readv
	size_t liovcnt = read_iovec->scattered() ? read_iovec->count() * read_count : 1;
	struct iovec local_iov[liovcnt];
	if (read_iovec->scattered()) {
		ptrdiff_t stride = read_iovec->stride();
		struct iovec *iov = local_iov;
		for (int j = read_skip; j < (read_skip + read_count); j++)
			for (auto &e: *read_iovec)
				*iov++ = { static_cast<uint8_t *>(const_cast<void *>(read_buf)) + j * stride + e.dis_, e.len_ };
	} else {
		local_iov[0] = { static_cast<uint8_t *>(const_cast<void *>(read_buf)) + read_skip * read_iovec->len(), total_bytes };
	}
	size_t riovcnt = write_iovec->scattered() ? write_iovec->count() * write_count : 1;
	struct iovec remote_iov[riovcnt];
	if (write_iovec->scattered()) {
		ptrdiff_t stride = write_iovec->stride();
		struct iovec *iov = remote_iov;
		for (int j = write_seek; j < (write_seek + write_count); j++)
			for (auto &e: *write_iovec)
				*iov++ = { static_cast<uint8_t *>(write_buf) + j * stride + e.dis_, e.len_ };
	} else {
		remote_iov[0] = { static_cast<uint8_t *>(write_buf) + write_seek * write_iovec->len(), total_bytes };
	}
	if ((ssize_t)total_bytes != process_vm_writev(umpi->self->pid_, local_iov, liovcnt, remote_iov, riovcnt, 0)) {
		perror("process_vm_writev");
		return 0;
	}
	return total_bytes;
}

int cookie::write_to_owner(const void *buf, iovec_pointer iovec, int count, int seek, int skip)
{
	size_t total_bytes = count * iovec->len();
	if (size_ * iovec_->len() < seek * iovec_->len() + total_bytes)
		return owner_->notify_done(this, MPI_FAIL);
	count_ = count_ * iovec_->len() < total_bytes ? count_ : total_bytes / iovec_->len();
#if 0
	if (iovec->contiguous() && iovec_->contiguous()) {
		struct iovec local = { static_cast<uint8_t *>(const_cast<void *>(buf)) + skip * iovec->len(), total_bytes };
		struct iovec remote = { static_cast<uint8_t *>(buf_) + seek * iovec_->len(), total_bytes };
		if ((ssize_t)total_bytes != process_vm_writev(owner_->pid_, &local, 1, &remote, 1, 0)) {
			perror("process_vm_writev");
			return owner_->notify_done(this, MPI_FAIL);
		}
		return owner_->notify_done(this, MPI_SUCCESS);
	}
#endif
	size_t liovcnt = iovec->scattered() ? iovec->count() * count : 1;
	struct iovec local_iov[liovcnt];
	if (iovec->scattered()) {
		ptrdiff_t stride = iovec->stride();
		struct iovec *iov = local_iov;
		for (int j = skip; j < (skip + count); j++)
			for (auto &e: *iovec)
				*iov++ = { static_cast<uint8_t *>(const_cast<void *>(buf)) + j * stride + e.dis_, e.len_ };
	} else {
		local_iov[0] = { static_cast<uint8_t *>(const_cast<void *>(buf)) + skip * iovec->len(), total_bytes };
	}
	size_t riovcnt = iovec_->scattered() ? iovec_->count() * (size_ - seek) : 1;
	struct iovec remote_iov[riovcnt];
	if (iovec_->scattered()) {
		ptrdiff_t stride = iovec_->stride();
		struct iovec *iov = remote_iov;
		for (int j = seek; j < size_; j++)
			for (auto &e: *iovec_)
				*iov++ = { static_cast<uint8_t *>(buf_) + j * stride + e.dis_, e.len_ };
	} else {
		remote_iov[0] = { static_cast<uint8_t *>(buf_) + seek * iovec_->len(), total_bytes };
	}
	if ((ssize_t)total_bytes != process_vm_writev(owner_->pid_, local_iov, liovcnt, remote_iov, riovcnt, 0)) {
		perror("process_vm_writev");
		return owner_->notify_done(this, MPI_FAIL);
	}
	return owner_->notify_done(this, MPI_SUCCESS);
}

int cookie::read_from_owner(void *buf, iovec_pointer iovec, int size, int count, int seek, int skip)
{
	count = count ? count : ((size_ - skip) * iovec_->len()) / iovec->len();
	size_t total_bytes = count * iovec->len();
	if (size_ * iovec_->len() < skip * iovec_->len() + total_bytes || size * iovec->len() < seek * iovec->len() + total_bytes)
		return owner_->notify_done(this, MPI_FAIL);
#if 0
	if (iovec->contiguous() && iovec_->contiguous()) {
		struct iovec local = { static_cast<uint8_t *>(buf) + seek * iovec->len(), total_bytes };
		struct iovec remote = { static_cast<uint8_t *>(buf_) + skip * iovec_->len(), total_bytes };
		if ((ssize_t)total_bytes != process_vm_readv(owner_->pid_, &local, 1, &remote, 1, 0)) {
			perror("process_vm_readv");
			return owner_->notify_done(this, MPI_FAIL);
		}
		return owner_->notify_done(this, MPI_SUCCESS);
	}
#endif
	size_t liovcnt = iovec->scattered() ? iovec->count() * count : 1;
	struct iovec local_iov[liovcnt];
	if (iovec->scattered()) {
		ptrdiff_t stride = iovec->stride();
		struct iovec *iov = local_iov;
		for (int j = seek; j < (seek + count); j++)
			for (auto &e: *iovec)
				*iov++ = { static_cast<uint8_t *>(const_cast<void *>(buf)) + j * stride + e.dis_, e.len_ };
	} else {
		local_iov[0] = { static_cast<uint8_t *>(const_cast<void *>(buf)) + seek * iovec->len(), total_bytes };
	}
	size_t riovcnt = iovec_->scattered() ? iovec_->count() * (size_ - skip) : 1;
	struct iovec remote_iov[riovcnt];
	if (iovec_->scattered()) {
		ptrdiff_t stride = iovec_->stride();
		struct iovec *iov = remote_iov;
		for (int j = skip; j < size_; j++)
			for (auto &e: *iovec_)
				*iov++ = { static_cast<uint8_t *>(buf_) + j * stride + e.dis_, e.len_ };
	} else {
		remote_iov[0] = { static_cast<uint8_t *>(buf_) + skip * iovec_->len(), total_bytes };
	}
	if ((ssize_t)total_bytes != process_vm_readv(owner_->pid_, local_iov, liovcnt, remote_iov, riovcnt, 0)) {
		perror("process_vm_readv");
		return owner_->notify_done(this, MPI_FAIL);
	}
	return owner_->notify_done(this, MPI_SUCCESS);
}

void process::move_to_pending(box_type &box, box_type::iterator cookie)
{
	mutex_.lock();
	pending_.splice(pending_.begin(), box, cookie);
	mutex_.unlock();
}

collect *shared::get_current_slot()
{
	int turn = umpi->ticket % UMPI_OUTSTANDING_COLLECTIVE_OPERATIONS;
	collect *slot = slots_ + turn;
	slot->mutex_.lock();
	if (slot->box_.empty()) {
		slot->ticket_ = umpi->ticket;
		slot->joined_ = 0;
	} else if (slot->ticket_ != umpi->ticket) {
		slot->mutex_.unlock();
		std::cerr << "too many outstanding requests." << std::endl;
		std::exit(1);
	}
	umpi->ticket++;
	slot->joined_++;
	return slot;
}

bool collect::joined_last()
{
	return joined_ == umpi->size;
}

bool collect::joined_first()
{
	return joined_ == 1;
}

int collect::bcast_request(MPI_Request *request, void *buf, int count, MPI_Datatype datatype, int root)
{
	auto cookie = box_.begin();
	if (cookie != box_.end() && cookie->src_ == root) {
		cookie->join();
		if (joined_last())
			cookie->owner_->move_to_pending(box_, cookie);
		mutex_.unlock();
		*request = &(*cookie);
		return cookie->read_from_owner(buf, datatype->iovec_, count, count);
	}
	*request = create_request(box_, buf, datatype->iovec_, count, count, umpi->rank, UMPI_TAG_CC);

	mutex_.unlock();
	return MPI_SUCCESS;
}

int collect::root_bcast_request(MPI_Request *request, const void *buf, int count, MPI_Datatype datatype)
{
	box_type todo;
	todo.splice(todo.begin(), box_);

	if (joined_last()) {
		cookie &borrow = todo.back();
		borrow.join();
		*request = &borrow;
	} else {
		*request = create_request(box_, const_cast<void *>(buf), datatype->iovec_, count, count, umpi->rank, UMPI_TAG_CC, umpi->size - joined_);
	}
	mutex_.unlock();

	for (auto cookie = todo.begin(); cookie != todo.end();) {
		auto pending = cookie++;
		pending->owner_->move_to_pending(todo, pending);
		int ret = pending->write_to_owner(buf, datatype->iovec_, count);
		if (ret)
			return ret;
	}
	return MPI_SUCCESS;
}

int MPI_Ibcast(void *buf, int count, MPI_Datatype datatype, int root, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || (!buf && count) || !datatype || root < 0 || umpi->size <= root || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	collect *slot = umpi->shared->get_current_slot();
	if (umpi->rank == root)
		return slot->root_bcast_request(request, buf, count, datatype);
	return slot->bcast_request(request, buf, count, datatype, root);
}

int collect::allgather_request(MPI_Request *request, const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype)
{
	if (joined_first()) {
		if (sendbuf != MPI_IN_PLACE)
			umpi_copy(sendbuf, sendtype->iovec_, sendcount, 0, recvbuf, recvtype->iovec_, recvcount, umpi->rank * recvcount);
		*request = create_request(box_, recvbuf, recvtype->iovec_, recvcount * umpi->size, recvcount, umpi->rank, UMPI_TAG_CC, umpi->size);
		mutex_.unlock();
		return MPI_SUCCESS;
	}

	int ret;
	auto root = box_.begin();
	if (sendbuf == MPI_IN_PLACE)
		ret = root->write_to_owner(recvbuf, recvtype->iovec_, recvcount, root->count_ * umpi->rank, recvcount * umpi->rank);
	else
		ret = root->write_to_owner(sendbuf, sendtype->iovec_, sendcount, root->count_ * umpi->rank);
	if (ret)
		return ret;

	if (!joined_last()) {
		*request = create_request(box_, recvbuf, recvtype->iovec_, recvcount * umpi->size, recvcount, umpi->rank, UMPI_TAG_CC);
		mutex_.unlock();
		return MPI_SUCCESS;
	}

	box_type todo;
	todo.splice(todo.begin(), box_);

	auto first = todo.begin();
	first->join();
	*request = &(*first);

	mutex_.unlock();

	first->owner_->move_to_pending(todo, first);
	first->read_from_owner(recvbuf, recvtype->iovec_, recvcount * umpi->size, recvcount * umpi->size);

	for (auto cookie = todo.begin(); cookie != todo.end();) {
		auto pending = cookie++;
		pending->owner_->move_to_pending(todo, pending);
		int ret = pending->write_to_owner(recvbuf, recvtype->iovec_, recvcount * umpi->size);
		if (ret)
			return ret;
	}
	return MPI_SUCCESS;
}

int MPI_Iallgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || (!sendbuf && sendcount) || (sendbuf != MPI_IN_PLACE && !sendtype) || (!recvbuf && recvcount) || !recvtype || (sendbuf != MPI_IN_PLACE && sendcount * sendtype->len() != recvcount * recvtype->len()) || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	return umpi->shared->get_current_slot()->allgather_request(request, sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype);
}

int collect::allreduce_request(MPI_Request *request, const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op)
{
	if (sendbuf == MPI_IN_PLACE) {
		if (joined_first()) {
			iovec_ = datatype->iovec_;
			count_ = count;
			umpi_copy(recvbuf, datatype->iovec_, count, 0, sendbuf_, iovec_, count_, 0);
		} else if (!joined_last()) {
			umpi_copy(sendbuf_, iovec_, count_, 0, recvbuf_, datatype->iovec_, count, 0);
			(*op)(recvbuf, recvbuf_, &count, &datatype);
			iovec_ = datatype->iovec_;
			count_ = count;
			umpi_copy(recvbuf_, iovec_, count_, 0, sendbuf_, iovec_, count_, 0);
		} else {
			umpi_copy(sendbuf_, iovec_, count_, 0, recvbuf_, datatype->iovec_, count, 0);
			(*op)(recvbuf_, recvbuf, &count, &datatype);
		}
	} else {
		if (joined_first()) {
			umpi_copy(sendbuf, datatype->iovec_, count, 0, recvbuf, datatype->iovec_, count, 0);
		} else {
			int ret = box_.back().read_from_owner(recvbuf, datatype->iovec_, count, count);
			if (ret)
				return ret;
			(*op)(sendbuf, recvbuf, &count, &datatype);
		}
	}

	if (!joined_last()) {
		*request = create_request(box_, recvbuf, datatype->iovec_, count, count, umpi->rank, UMPI_TAG_CC, sendbuf == MPI_IN_PLACE ? 1 : 2);
		mutex_.unlock();
		return MPI_SUCCESS;
	}

	box_type todo;
	todo.splice(todo.begin(), box_);

	cookie &borrow = todo.back();
	borrow.join();
	*request = &borrow;

	mutex_.unlock();

	for (auto cookie = todo.begin(); cookie != todo.end();) {
		auto pending = cookie++;
		pending->owner_->move_to_pending(todo, pending);
		int ret = pending->write_to_owner(recvbuf, datatype->iovec_, count);
		if (ret)
			return ret;
	}
	return MPI_SUCCESS;
}

int MPI_Iallreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || ((!sendbuf || !recvbuf) && count) || !datatype || !op || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	if (sendbuf == MPI_IN_PLACE && UMPI_MAX_IN_PLACE_LEN < count * datatype->stride())
		return MPI_FAIL;
	return umpi->shared->get_current_slot()->allreduce_request(request, sendbuf, recvbuf, count, datatype, op);
}

int collect::exscan_request(MPI_Request *request, const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op)
{
	// cant do better at the moment .. concept sucks.
	if (UMPI_MAX_IN_PLACE_LEN < count * datatype->stride())
		return MPI_FAIL;

	if (umpi->rank && sendbuf != MPI_IN_PLACE)
		umpi_copy(sendbuf, datatype->iovec_, count, 0, recvbuf, datatype->iovec_, count, 0);

	*request = create_request(box_, umpi->rank ? recvbuf : const_cast<void *>(sendbuf), datatype->iovec_, count, count, umpi->rank, UMPI_TAG_CC, umpi->rank ? 2 : 1);

	if (!joined_last()) {
		mutex_.unlock();
		return MPI_SUCCESS;
	}

	box_type todo;
	todo.splice(todo.begin(), box_);

	mutex_.unlock();

	todo.sort([](struct cookie &left, struct cookie &right){ return left.src_ < right.src_; });

	auto first = todo.begin();
	first->owner_->move_to_pending(todo, first);
	int ret = first->read_from_owner(sendbuf_, datatype->iovec_, count);
	if (ret)
		return ret;

	for (auto cookie = todo.begin(); cookie != todo.end();) {
		auto pending = cookie++;
		pending->owner_->move_to_pending(todo, pending);
		ret = pending->read_from_owner(recvbuf_, datatype->iovec_, count);
		if (ret)
			return ret;
		ret = pending->write_to_owner(sendbuf_, datatype->iovec_, count);
		if (ret)
			return ret;
		if (cookie != todo.end())
			(*op)(recvbuf_, sendbuf_, &count, &datatype);
	}
	return MPI_SUCCESS;
}

int MPI_Iexscan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || ((!sendbuf || (umpi->rank && !recvbuf)) && count) || !datatype || !op || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	if (sendbuf == MPI_IN_PLACE && UMPI_MAX_IN_PLACE_LEN < count * datatype->stride())
		return MPI_FAIL;
	return umpi->shared->get_current_slot()->exscan_request(request, sendbuf, recvbuf, count, datatype, op);
}

int collect::scan_request(MPI_Request *request, const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op)
{
	// cant do better at the moment .. concept sucks.
	if (UMPI_MAX_IN_PLACE_LEN < count * datatype->stride())
		return MPI_FAIL;

	if (sendbuf != MPI_IN_PLACE)
		umpi_copy(sendbuf, datatype->iovec_, count, 0, recvbuf, datatype->iovec_, count, 0);

	*request = create_request(box_, recvbuf, datatype->iovec_, count, count, umpi->rank, UMPI_TAG_CC, umpi->rank ? 2 : 1);

	if (!joined_last()) {
		mutex_.unlock();
		return MPI_SUCCESS;
	}

	box_type todo;
	todo.splice(todo.begin(), box_);

	mutex_.unlock();

	todo.sort([](struct cookie &left, struct cookie &right){ return left.src_ < right.src_; });

	auto first = todo.begin();
	first->owner_->move_to_pending(todo, first);
	int ret = first->read_from_owner(sendbuf_, datatype->iovec_, count);
	if (ret)
		return ret;

	for (auto cookie = todo.begin(); cookie != todo.end();) {
		auto pending = cookie++;
		pending->owner_->move_to_pending(todo, pending);
		ret = pending->read_from_owner(recvbuf_, datatype->iovec_, count);
		if (ret)
			return ret;
		(*op)(recvbuf_, sendbuf_, &count, &datatype);
		ret = pending->write_to_owner(sendbuf_, datatype->iovec_, count);
		if (ret)
			return ret;
	}
	return MPI_SUCCESS;
}

int MPI_Iscan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || ((!sendbuf || !recvbuf) && count) || !datatype || !op || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	if (sendbuf == MPI_IN_PLACE && UMPI_MAX_IN_PLACE_LEN < count * datatype->stride())
		return MPI_FAIL;
	return umpi->shared->get_current_slot()->scan_request(request, sendbuf, recvbuf, count, datatype, op);
}

int collect::gather_request(MPI_Request *request, const void *sendbuf, int sendcount, MPI_Datatype sendtype, int root)
{
	auto cookie = box_.begin();
	if (cookie != box_.end() && cookie->src_ == root) {
		cookie->join();
		if (joined_last())
			cookie->owner_->move_to_pending(box_, cookie);
		mutex_.unlock();
		*request = &(*cookie);
		return cookie->write_to_owner(sendbuf, sendtype->iovec_, sendcount, cookie->count_ * umpi->rank);
	}
	*request = create_request(box_, const_cast<void *>(sendbuf), sendtype->iovec_, sendcount, sendcount, umpi->rank, UMPI_TAG_CC);

	mutex_.unlock();
	return MPI_SUCCESS;
}

int collect::root_gather_request(MPI_Request *request, void *recvbuf, int recvcount, MPI_Datatype recvtype)
{
	box_type todo;
	todo.splice(todo.begin(), box_);

	if (joined_last()) {
		cookie &borrow = todo.back();
		borrow.join();
		*request = &borrow;
	} else {
		*request = create_request(box_, recvbuf, recvtype->iovec_, recvcount * umpi->size, recvcount, umpi->rank, UMPI_TAG_CC, umpi->size - joined_);
	}
	mutex_.unlock();

	for (auto cookie = todo.begin(); cookie != todo.end();) {
		auto pending = cookie++;
		pending->owner_->move_to_pending(todo, pending);
		int ret = pending->read_from_owner(recvbuf, recvtype->iovec_, recvcount * umpi->size, recvcount, pending->src_);
		if (ret)
			return ret;
	}
	return MPI_SUCCESS;
}

int MPI_Igather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || (!sendbuf && sendcount) || (sendbuf != MPI_IN_PLACE && !sendtype) || root < 0 || umpi->size <= root || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	collect *slot = umpi->shared->get_current_slot();
	if (umpi->rank == root) {
		if ((!recvbuf && recvcount) || !recvtype || (sendbuf != MPI_IN_PLACE && sendcount * sendtype->len() != recvcount * recvtype->len()))
			return MPI_FAIL;
		if (sendbuf != MPI_IN_PLACE)
			umpi_copy(sendbuf, sendtype->iovec_, sendcount, 0, recvbuf, recvtype->iovec_, recvcount, umpi->rank * recvcount);
		return slot->root_gather_request(request, recvbuf, recvcount, recvtype);
	}
	return slot->gather_request(request, sendbuf, sendcount, sendtype, root);
}

int collect::gatherv_request(MPI_Request *request, const void *sendbuf, int sendcount, MPI_Datatype sendtype, int root)
{
	auto cookie = box_.begin();
	if (cookie != box_.end() && cookie->src_ == root) {
		cookie->join();
		if (joined_last())
			cookie->owner_->move_to_pending(box_, cookie);
		mutex_.unlock();
		*request = &(*cookie);
		return cookie->write_to_owner(sendbuf, sendtype->iovec_, sendcount, displs_[umpi->rank]);
	}
	*request = create_request(box_, const_cast<void *>(sendbuf), sendtype->iovec_, sendcount, sendcount, umpi->rank, UMPI_TAG_CC);

	mutex_.unlock();
	return MPI_SUCCESS;
}

int collect::root_gatherv_request(MPI_Request *request, void *recvbuf, const int *recvcounts, const int *displs, MPI_Datatype recvtype)
{
	int totalcount = 0;
	for (int i = 0; i < umpi->size; i++)
		totalcount += recvcounts[i];

	memcpy(&(*displs_), displs, sizeof(int) * umpi->size);

	box_type todo;
	todo.splice(todo.begin(), box_);

	if (joined_last()) {
		cookie &borrow = todo.back();
		borrow.join();
		*request = &borrow;
	} else {
		*request = create_request(box_, recvbuf, recvtype->iovec_, totalcount, 0, umpi->rank, UMPI_TAG_CC, umpi->size - joined_);
	}
	mutex_.unlock();

	for (auto cookie = todo.begin(); cookie != todo.end();) {
		auto pending = cookie++;
		pending->owner_->move_to_pending(todo, pending);
		int ret = pending->read_from_owner(recvbuf, recvtype->iovec_, totalcount, recvcounts[pending->src_], displs[pending->src_]);
		if (ret)
			return ret;
	}
	return MPI_SUCCESS;
}

int MPI_Igatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, const int *recvcounts, const int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || (!sendbuf && sendcount) || (sendbuf != MPI_IN_PLACE && !sendtype) || root < 0 || umpi->size <= root || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	collect *slot = umpi->shared->get_current_slot();
	if (umpi->rank == root) {
		if (!recvbuf || !recvcounts || !displs || !recvtype || (sendbuf != MPI_IN_PLACE && sendcount * sendtype->len() != recvcounts[umpi->rank] * recvtype->len()))
			return MPI_FAIL;
		if (sendbuf != MPI_IN_PLACE)
			umpi_copy(sendbuf, sendtype->iovec_, sendcount, 0, recvbuf, recvtype->iovec_, recvcounts[umpi->rank], displs[umpi->rank]);
		return slot->root_gatherv_request(request, recvbuf, recvcounts, displs, recvtype);
	}
	return slot->gatherv_request(request, sendbuf, sendcount, sendtype, root);
}

int collect::scatter_request(MPI_Request *request, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root)
{
	auto cookie = box_.begin();
	if (cookie != box_.end() && cookie->src_ == root) {
		cookie->join();
		if (joined_last())
			cookie->owner_->move_to_pending(box_, cookie);
		mutex_.unlock();
		*request = &(*cookie);
		return cookie->read_from_owner(recvbuf, recvtype->iovec_, recvcount, recvcount, 0, cookie->count_ * umpi->rank);
	}
	*request = create_request(box_, recvbuf, recvtype->iovec_, recvcount, recvcount, umpi->rank, UMPI_TAG_CC);

	mutex_.unlock();
	return MPI_SUCCESS;
}

int collect::root_scatter_request(MPI_Request *request, const void *sendbuf, int sendcount, MPI_Datatype sendtype)
{
	box_type todo;
	todo.splice(todo.begin(), box_);

	if (joined_last()) {
		cookie &borrow = todo.back();
		borrow.join();
		*request = &borrow;
	} else {
		*request = create_request(box_, const_cast<void *>(sendbuf), sendtype->iovec_, sendcount * umpi->size, sendcount, umpi->rank, UMPI_TAG_CC, umpi->size - joined_);
	}
	mutex_.unlock();

	for (auto cookie = todo.begin(); cookie != todo.end();) {
		auto pending = cookie++;
		pending->owner_->move_to_pending(todo, pending);
		int ret = pending->write_to_owner(sendbuf, sendtype->iovec_, sendcount, 0, sendcount * pending->src_);
		if (ret)
			return ret;
	}
	return MPI_SUCCESS;
}

int MPI_Iscatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || (!recvbuf && recvcount) || (recvbuf != MPI_IN_PLACE && !recvtype) || root < 0 || umpi->size <= root || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	collect *slot = umpi->shared->get_current_slot();
	if (umpi->rank == root) {
		if ((!sendbuf && sendcount) || !sendtype || (recvbuf != MPI_IN_PLACE && sendcount * sendtype->len() != recvcount * recvtype->len()))
			return MPI_FAIL;
		if (recvbuf != MPI_IN_PLACE)
			umpi_copy(sendbuf, sendtype->iovec_, sendcount, umpi->rank * sendcount, recvbuf, recvtype->iovec_, recvcount, 0);
		return slot->root_scatter_request(request, sendbuf, sendcount, sendtype);
	}
	return slot->scatter_request(request, recvbuf, recvcount, recvtype, root);
}

bool cookie::recv_match(int source, int tag)
{
	return (tag_ == tag || (tag == MPI_ANY_TAG && tag_ >= 0)) && (src_ == source || source == MPI_ANY_SOURCE);
}

bool cookie::send_match(int source, int tag)
{
	return (tag_ == tag || tag_ == MPI_ANY_TAG) && (src_ == source || src_ == MPI_ANY_SOURCE);
}

int process::recv_request(MPI_Request *request, void *buf, int count, MPI_Datatype datatype, int source, int tag)
{
	mutex_.lock();
	for (auto cookie = inbox_.begin(); cookie != inbox_.end(); ++cookie) {
		if (cookie->recv_match(source, tag)) {
			pending_.splice(pending_.begin(), inbox_, cookie);
			cookie->join();
			mutex_.unlock();
			*request = &(*cookie);
			return cookie->read_from_owner(buf, datatype->iovec_, count);
		}
	}
	*request = create_request(outbox_, buf, datatype->iovec_, count, count, source, tag);
	mutex_.unlock();
	return MPI_SUCCESS;
}

int process::send_request(MPI_Request *request, const void *buf, int count, MPI_Datatype datatype, int tag)
{
	mutex_.lock();
	for (auto cookie = outbox_.begin(); cookie != outbox_.end(); ++cookie) {
		if (cookie->send_match(umpi->rank, tag)) {
			pending_.splice(pending_.begin(), outbox_, cookie);
			cookie->src_ = umpi->rank;
			cookie->tag_ = tag;
			cookie->join();
			mutex_.unlock();
			*request = &(*cookie);
			return cookie->write_to_owner(buf, datatype->iovec_, count);
		}
	}
	*request = create_request(inbox_, const_cast<void *>(buf), datatype->iovec_, count, count, umpi->rank, tag);
	if (waiting_for_ == waiting_for_incoming)
		cond_.notify_one();
	mutex_.unlock();
	return MPI_SUCCESS;
}

int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || (!buf && count) || !datatype || source < MPI_ANY_SOURCE || umpi->size <= source || tag < MPI_ANY_TAG || comm != MPI_COMM_WORLD || !request)
		return MPI_FAIL;
	return umpi->self->recv_request(request, buf, count, datatype, source, tag);
}

int MPI_Issend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || (!buf && count) || !datatype || dest < 0 || umpi->size <= dest || tag < 0 || comm != MPI_COMM_WORLD || !request)
		return MPI_FAIL;
	return umpi->procs[dest].send_request(request, buf, count, datatype, tag);
}

int MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request)
{
	return MPI_Issend(buf, count, datatype, dest, tag, comm, request);
}

void cookie::leave()
{
	owner_->finalize(this);
}

void process::wait(cookie_pointer cookie)
{
	mutex_.lock();
	waiting_ = cookie;
	waiting_for_ = waiting_for_cookie;
	while (cookie->busy_)
		cond_.wait(mutex_);
	waiting_ = nullptr;
	waiting_for_ = not_waiting;
	mutex_.unlock();
}

int process::wait_any(int count, umpi_request **cookies)
{
	mutex_.lock();
	waiting_for_ = waiting_for_any_cookie;
	while (true) {
		for (int i = 0; i < count; i++) {
			struct cookie *cookie = static_cast<struct cookie *>(cookies[i]);
			if (!cookie || cookie->busy_)
				continue;
			waiting_for_ = not_waiting;
			mutex_.unlock();
			return i;
		}
		cond_.wait(mutex_);
	}
}

bool cookie::owner() const
{
	return umpi->self == &(*owner_);
}

static int get_status(MPI_Status *status, struct cookie *cookie)
{
	if (status) {
		status->MPI_SOURCE = cookie->src_;
		status->MPI_TAG = cookie->tag_;
		status->len = cookie->count_ * cookie->iovec_->len();
	}
	return cookie->err_;
}

static int empty_status(MPI_Status *status)
{
	if (status) {
		status->MPI_SOURCE = MPI_ANY_SOURCE;
		status->MPI_TAG = MPI_ANY_TAG;
		status->MPI_ERROR = MPI_SUCCESS;
		status->len = 0;
	}
	return MPI_SUCCESS;
}

int MPI_Waitany(int count, MPI_Request *array_of_requests, int *index, MPI_Status *status)
{
	if (!umpi || (count && !array_of_requests) || !index)
		return MPI_FAIL;

	int begin = count;
	int end = 0;
	for (*index = 0; *index < count; (*index)++) {
		struct cookie *cookie = static_cast<struct cookie *>(array_of_requests[*index]);
		if (!cookie)
			continue;
		if (!cookie->owner())
			goto end;
		begin = std::min(begin, *index);
		end = *index + 1;
	}

	*index = MPI_UNDEFINED;
	if (!end)
		return empty_status(status);

	*index = umpi->self->wait_any(end - begin, array_of_requests + begin);

end:
	struct cookie *cookie = static_cast<struct cookie *>(array_of_requests[*index]);
	array_of_requests[*index] = nullptr;
	int err = get_status(status, cookie);
	cookie->leave();
	return err;
}

void cookie::wait()
{
	if (umpi->self == &(*owner_))
		umpi->self->wait(this);
}

int MPI_Wait(MPI_Request *request, MPI_Status *status)
{
	if (!umpi)
		return MPI_FAIL;
	if (!request || !*request)
		return empty_status(status);
	struct cookie *cookie = static_cast<struct cookie *>(*request);
	*request = nullptr;
	cookie->wait();
	int err = get_status(status, cookie);
	cookie->leave();
	return err;
}

cookie *process::wait_any(int source, int tag)
{
	mutex_.lock();
	waiting_for_ = waiting_for_incoming;
	while (inbox_.empty())
		cond_.wait(mutex_);
	box_type::iterator cookie;
	for (cookie = inbox_.begin(); !cookie->recv_match(source, tag); cookie++) {
		while (&(*cookie) == &inbox_.back())
			cond_.wait(mutex_);
	}
	waiting_for_ = not_waiting;
	mutex_.unlock();
	return &(*cookie);
}

int MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status)
{
	if (!umpi || source < MPI_ANY_SOURCE || umpi->size <= source || tag < MPI_ANY_TAG || comm != MPI_COMM_WORLD || !status)
		return MPI_FAIL;
	struct cookie *cookie = umpi->self->wait_any(source, tag);
	return get_status(status, cookie);
}

cookie *process::find_any(int source, int tag)
{
	mutex_.lock();
	for (auto cookie = inbox_.begin(); cookie != inbox_.end(); cookie++) {
		if (cookie->recv_match(source, tag)) {
			mutex_.unlock();
			return &(*cookie);
		}
	}
	mutex_.unlock();
	return nullptr;
}

int MPI_Iprobe(int source, int tag, MPI_Comm comm, int *flag, MPI_Status *status)
{
	if (!umpi || source < MPI_ANY_SOURCE || umpi->size <= source || tag < MPI_ANY_TAG || comm != MPI_COMM_WORLD || !flag || !status)
		return MPI_FAIL;
	struct cookie *cookie = umpi->self->find_any(source, tag);
	if (!cookie) {
		*flag = 0;
		return MPI_SUCCESS;
	}
	*flag = 1;
	return get_status(status, cookie);
}

int MPI_Test(MPI_Request *request, int *flag, MPI_Status *status)
{
	if (!umpi || !flag)
		return MPI_FAIL;
	if (!request || !*request)
		return empty_status(status);
	struct cookie *cookie = static_cast<struct cookie *>(*request);
	if (cookie->busy_) {
		*flag = 0;
		return MPI_SUCCESS;
	}
	*flag = 1;
	*request = nullptr;
	int err = get_status(status, cookie);
	cookie->leave();
	return err;
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
{
	MPI_Request request;
	int ret = MPI_Irecv(buf, count, datatype, source, tag, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, status);
}

int MPI_Ssend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
{
	MPI_Request request;
	int ret = MPI_Issend(buf, count, datatype, dest, tag, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, nullptr);
}

int MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
{
	return MPI_Ssend(buf, count, datatype, dest, tag, comm);
}

int MPI_Bcast(void *buf, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
{
	MPI_Request request;
	int ret = MPI_Ibcast(buf, count, datatype, root, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, nullptr);
}

int MPI_Allgather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm)
{
	MPI_Request request;
	int ret = MPI_Iallgather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, nullptr);
}

int MPI_Allreduce(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
{
	if (!umpi || ((!sendbuf || !recvbuf) && count) || !datatype || !op || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	if (umpi->size == 1) {
		if (sendbuf != MPI_IN_PLACE)
			umpi_copy(sendbuf, datatype->iovec_, count, 0, recvbuf, datatype->iovec_, count, 0);
		return MPI_SUCCESS;
	}
	MPI_Request request;
	int ret = MPI_Iallreduce(sendbuf, recvbuf, count, datatype, op, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, nullptr);
}

int MPI_Exscan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
{
	MPI_Request request;
	int ret = MPI_Iexscan(sendbuf, recvbuf, count, datatype, op, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, nullptr);
}

int MPI_Scan(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm)
{
	MPI_Request request;
	int ret = MPI_Iscan(sendbuf, recvbuf, count, datatype, op, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, nullptr);
}

int MPI_Gather(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
{
	MPI_Request request;
	int ret = MPI_Igather(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, nullptr);
}

int MPI_Gatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, const int *recvcounts, const int *displs, MPI_Datatype recvtype, int root, MPI_Comm comm)
{
	MPI_Request request;
	int ret = MPI_Igatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype, root, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, nullptr);
}

int MPI_Allgatherv(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, const int *recvcounts, const int *displs, MPI_Datatype recvtype, MPI_Comm comm)
{
	for (int root = 0; root < umpi->size; root++) {
		int ret;
		if (sendbuf == MPI_IN_PLACE && root != umpi->rank)
			ret = MPI_Gatherv(static_cast<uint8_t *>(recvbuf) + displs[umpi->rank] * recvtype->stride(), recvcounts[umpi->rank], recvtype, 0, 0, 0, 0, root, comm);
		else
			ret = MPI_Gatherv(sendbuf, sendcount, sendtype, recvbuf, recvcounts, displs, recvtype, root, comm);
		if (ret)
			return ret;
		ret = MPI_Barrier(comm);
		if (ret)
			return ret;
	}
	return MPI_SUCCESS;
}

int MPI_Scatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm)
{
	MPI_Request request;
	int ret = MPI_Iscatter(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm, &request);
	if (ret)
		return ret;
	return MPI_Wait(&request, nullptr);
}

int MPI_Get_count(const MPI_Status *status, MPI_Datatype datatype, int *count)
{
	if (!umpi || !status || !datatype || !count)
		return MPI_FAIL;
	*count = status->len / datatype->len();
	return MPI_SUCCESS;
}

int MPI_Waitall(int count, MPI_Request *array_of_requests, MPI_Status *array_of_statuses)
{
	if (array_of_statuses) {
		for (int i = 0; i < count; i++) {
			array_of_statuses[i].MPI_ERROR = MPI_Wait(array_of_requests + i, array_of_statuses + i);
			if (array_of_statuses[i].MPI_ERROR)
				return MPI_ERR_IN_STATUS;
		}
	} else {
		for (int i = 0; i < count; i++) {
			int error = MPI_Wait(array_of_requests + i, nullptr);
			if (error)
				return error;
		}
	}
	return MPI_SUCCESS;
}

int MPI_Pack(const void *inbuf, int incount, MPI_Datatype datatype, void *outbuf, int outsize, int *position, MPI_Comm comm)
{
	if (!umpi || (!inbuf && incount) || !datatype || datatype->scattered() || (!outbuf && outsize) || !position || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	int len = incount * datatype->len();
	if (outsize < len + *position)
		return MPI_FAIL;
	memcpy(static_cast<uint8_t *>(outbuf) + *position, inbuf, len);
	*position += len;
	return MPI_SUCCESS;
}

int MPI_Unpack(const void *inbuf, int insize, int *position, void *outbuf, int outcount, MPI_Datatype datatype, MPI_Comm comm)
{
	if (!umpi || (!inbuf && insize) || !position || (!outbuf && outcount) || !datatype || datatype->scattered() || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	int len = outcount * datatype->len();
	if (insize < len + *position)
		return MPI_FAIL;
	memcpy(outbuf, static_cast<const uint8_t *>(inbuf) + *position, len);
	*position += len;
	return MPI_SUCCESS;
}

int MPI_Pack_size(int incount, MPI_Datatype datatype, MPI_Comm comm, int *size)
{
	if (!umpi || !datatype || datatype->scattered() || comm != MPI_COMM_WORLD || !size)
		return MPI_FAIL;
	*size = incount * datatype->len();
	return MPI_SUCCESS;
}

