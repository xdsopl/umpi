/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "include/umpi.h"
#include "umpi.hh"
#include "funcs.hh"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/uio.h>

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
	char *name = getenv("UMPI_MMAP");
	if (!name) {
		fprintf(stderr, "env name UMPI_MMAP not set!\n");
		return MPI_FAIL;
	}
	int size = atoi(getenv("UMPI_SIZE"));
	int rank = atoi(getenv("UMPI_RANK"));
	int fd = open(name, O_RDWR);
	if (fd == -1) {
		perror("open");
		return MPI_FAIL;
	}
	mmap_len = calc_mmap_len(size);
	mmap_addr = mmap(0, mmap_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	if (mmap_addr == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return MPI_FAIL;
	}
	if (close(fd) == -1) {
		perror ("close");
		return MPI_FAIL;
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
	if (!cookie->busy_ && waiting_ == cookie)
		done_.notify_one();
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
			static_cast<uint8_t *>(write_buf) + write_seek * total_bytes,
			static_cast<const uint8_t *>(read_buf) + read_skip * total_bytes,
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
		for (int j = read_skip * read_count; j < (read_skip + 1) * read_count; j++)
			for (auto &e: *read_iovec)
				*iov++ = { static_cast<uint8_t *>(const_cast<void *>(read_buf)) + j * stride + e.dis_, e.len_ };
	} else {
		local_iov[0] = { static_cast<uint8_t *>(const_cast<void *>(read_buf)) + read_skip * total_bytes, total_bytes };
	}
	size_t riovcnt = write_iovec->scattered() ? write_iovec->count() * write_count : 1;
	struct iovec remote_iov[riovcnt];
	if (write_iovec->scattered()) {
		ptrdiff_t stride = write_iovec->stride();
		struct iovec *iov = remote_iov;
		for (int j = write_seek * write_count; j < (write_seek + 1) * write_count; j++)
			for (auto &e: *write_iovec)
				*iov++ = { static_cast<uint8_t *>(write_buf) + j * stride + e.dis_, e.len_ };
	} else {
		remote_iov[0] = { static_cast<uint8_t *>(write_buf) + write_seek * total_bytes, total_bytes };
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
	if (size_ * iovec_->len() < seek * count_ * iovec_->len() + total_bytes)
		return owner_->notify_done(this, MPI_FAIL);
#if 0
	if (iovec->contiguous() && iovec_->contiguous()) {
		struct iovec local = { static_cast<uint8_t *>(const_cast<void *>(buf)) + skip * total_bytes, total_bytes };
		struct iovec remote = { static_cast<uint8_t *>(buf_) + seek * total_bytes, total_bytes };
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
		for (int j = skip * count; j < (skip + 1) * count; j++)
			for (auto &e: *iovec)
				*iov++ = { static_cast<uint8_t *>(const_cast<void *>(buf)) + j * stride + e.dis_, e.len_ };
	} else {
		local_iov[0] = { static_cast<uint8_t *>(const_cast<void *>(buf)) + skip * total_bytes, total_bytes };
	}
	size_t riovcnt = iovec_->scattered() ? iovec_->count() * (size_ - seek * count_) : 1;
	struct iovec remote_iov[riovcnt];
	if (iovec_->scattered()) {
		ptrdiff_t stride = iovec_->stride();
		struct iovec *iov = remote_iov;
		for (int j = seek * count_; j < size_; j++)
			for (auto &e: *iovec_)
				*iov++ = { static_cast<uint8_t *>(buf_) + j * stride + e.dis_, e.len_ };
	} else {
		remote_iov[0] = { static_cast<uint8_t *>(buf_) + seek * total_bytes, total_bytes };
	}
	if ((ssize_t)total_bytes != process_vm_writev(owner_->pid_, local_iov, liovcnt, remote_iov, riovcnt, 0)) {
		perror("process_vm_writev");
		return owner_->notify_done(this, MPI_FAIL);
	}
	return owner_->notify_done(this, MPI_SUCCESS);
}

int cookie::read_from_owner(void *buf, iovec_pointer iovec, int size, int count, int seek, int skip)
{
	count = count ? count : ((size_ - skip * count_) * iovec_->len()) / iovec->len();
	size_t total_bytes = count * iovec->len();
	if (size_ * iovec_->len() < skip * count_ * iovec_->len() + total_bytes || size * iovec->len() < seek * count * iovec->len() + total_bytes)
		return owner_->notify_done(this, MPI_FAIL);
#if 0
	if (iovec->contiguous() && iovec_->contiguous()) {
		struct iovec local = { static_cast<uint8_t *>(buf) + seek * total_bytes, total_bytes };
		struct iovec remote = { static_cast<uint8_t *>(buf_) + skip * total_bytes, total_bytes };
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
		for (int j = seek * count; j < (seek + 1) * count; j++)
			for (auto &e: *iovec)
				*iov++ = { static_cast<uint8_t *>(const_cast<void *>(buf)) + j * stride + e.dis_, e.len_ };
	} else {
		local_iov[0] = { static_cast<uint8_t *>(const_cast<void *>(buf)) + seek * total_bytes, total_bytes };
	}
	size_t riovcnt = iovec_->scattered() ? iovec_->count() * (size_ - skip * count_) : 1;
	struct iovec remote_iov[riovcnt];
	if (iovec_->scattered()) {
		ptrdiff_t stride = iovec_->stride();
		struct iovec *iov = remote_iov;
		for (int j = skip * count_; j < size_; j++)
			for (auto &e: *iovec_)
				*iov++ = { static_cast<uint8_t *>(buf_) + j * stride + e.dis_, e.len_ };
	} else {
		remote_iov[0] = { static_cast<uint8_t *>(buf_) + skip * total_bytes, total_bytes };
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
	int turn = umpi->ticket % UMPI_OUTSTANDING_REQUESTS_PER_PROCESS;
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
	if (cookie != box_.end() && cookie->src_ == root && cookie->tag_ == UMPI_TAG_BCAST) {
		cookie->join();
		if (joined_last())
			cookie->owner_->move_to_pending(box_, cookie);
		mutex_.unlock();
		*request = &(*cookie);
		return cookie->read_from_owner(buf, datatype->iovec_, count, count);
	}
	*request = create_request(box_, buf, datatype->iovec_, count, count, umpi->rank, UMPI_TAG_BCAST);

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
		*request = create_request(box_, const_cast<void *>(buf), datatype->iovec_, count, count, umpi->rank, UMPI_TAG_BCAST, umpi->size - joined_);
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
		umpi_copy(sendbuf, sendtype->iovec_, sendcount, 0, recvbuf, recvtype->iovec_, recvcount, umpi->rank);
		*request = create_request(box_, recvbuf, recvtype->iovec_, recvcount * umpi->size, recvcount, umpi->rank, UMPI_TAG_ALLGATHER, umpi->size);
		mutex_.unlock();
		return MPI_SUCCESS;
	}

	int ret = box_.front().write_to_owner(sendbuf, sendtype->iovec_, sendcount, umpi->rank);
	if (ret)
		return ret;

	if (!joined_last()) {
		*request = create_request(box_, recvbuf, recvtype->iovec_, recvcount * umpi->size, recvcount, umpi->rank, UMPI_TAG_ALLGATHER);
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
	if (!umpi || (!sendbuf && sendcount) || !sendtype || (!recvbuf && recvcount) || !recvtype || sendcount * sendtype->len() != recvcount * recvtype->len() || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	return umpi->shared->get_current_slot()->allgather_request(request, sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype);
}

int collect::allreduce_request(MPI_Request *request, const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype, MPI_Op op)
{
	if (joined_first()) {
		umpi_copy(sendbuf, datatype->iovec_, count, 0, recvbuf, datatype->iovec_, count, 0);
	} else {
		int ret = box_.back().read_from_owner(recvbuf, datatype->iovec_, count, count);
		if (ret)
			return ret;
		(*op)(sendbuf, recvbuf, &count, &datatype);
	}

	if (!joined_last()) {
		*request = create_request(box_, recvbuf, datatype->iovec_, count, count, umpi->rank, UMPI_TAG_ALLREDUCE, 2);
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
	return umpi->shared->get_current_slot()->allreduce_request(request, sendbuf, recvbuf, count, datatype, op);
}

int collect::gather_request(MPI_Request *request, const void *sendbuf, int sendcount, MPI_Datatype sendtype, int root)
{
	auto cookie = box_.begin();
	if (cookie != box_.end() && cookie->src_ == root && cookie->tag_ == UMPI_TAG_GATHER) {
		cookie->join();
		if (joined_last())
			cookie->owner_->move_to_pending(box_, cookie);
		mutex_.unlock();
		*request = &(*cookie);
		return cookie->write_to_owner(sendbuf, sendtype->iovec_, sendcount, umpi->rank);
	}
	*request = create_request(box_, const_cast<void *>(sendbuf), sendtype->iovec_, sendcount, sendcount, umpi->rank, UMPI_TAG_GATHER);

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
		*request = create_request(box_, recvbuf, recvtype->iovec_, recvcount * umpi->size, recvcount, umpi->rank, UMPI_TAG_GATHER, umpi->size - joined_);
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
	if (!umpi || (!sendbuf && sendcount) || !sendtype || root < 0 || umpi->size <= root || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	collect *slot = umpi->shared->get_current_slot();
	if (umpi->rank == root) {
		if ((!recvbuf && recvcount) || !recvtype || sendcount * sendtype->len() != recvcount * recvtype->len())
			return MPI_FAIL;
		umpi_copy(sendbuf, sendtype->iovec_, sendcount, 0, recvbuf, recvtype->iovec_, recvcount, umpi->rank);
		return slot->root_gather_request(request, recvbuf, recvcount, recvtype);
	}
	return slot->gather_request(request, sendbuf, sendcount, sendtype, root);
}

int collect::scatter_request(MPI_Request *request, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root)
{
	auto cookie = box_.begin();
	if (cookie != box_.end() && cookie->src_ == root && cookie->tag_ == UMPI_TAG_SCATTER) {
		cookie->join();
		if (joined_last())
			cookie->owner_->move_to_pending(box_, cookie);
		mutex_.unlock();
		*request = &(*cookie);
		return cookie->read_from_owner(recvbuf, recvtype->iovec_, recvcount, recvcount, 0, umpi->rank);
	}
	*request = create_request(box_, recvbuf, recvtype->iovec_, recvcount, recvcount, umpi->rank, UMPI_TAG_SCATTER);

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
		*request = create_request(box_, const_cast<void *>(sendbuf), sendtype->iovec_, sendcount * umpi->size, sendcount, umpi->rank, UMPI_TAG_SCATTER, umpi->size - joined_);
	}
	mutex_.unlock();

	for (auto cookie = todo.begin(); cookie != todo.end();) {
		auto pending = cookie++;
		pending->owner_->move_to_pending(todo, pending);
		int ret = pending->write_to_owner(sendbuf, sendtype->iovec_, sendcount, 0, pending->src_);
		if (ret)
			return ret;
	}
	return MPI_SUCCESS;
}

int MPI_Iscatter(const void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm, MPI_Request *request)
{
	if (!umpi || (!recvbuf && recvcount) || !recvtype || root < 0 || umpi->size <= root || comm != MPI_COMM_WORLD)
		return MPI_FAIL;
	collect *slot = umpi->shared->get_current_slot();
	if (umpi->rank == root) {
		if ((!sendbuf && sendcount) || !sendtype || sendcount * sendtype->len() != recvcount * recvtype->len())
			return MPI_FAIL;
		umpi_copy(sendbuf, sendtype->iovec_, sendcount, umpi->rank, recvbuf, recvtype->iovec_, recvcount, 0);
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
	if (waiting_any_)
		incoming_.notify_one();
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

int process::wait(cookie_pointer cookie)
{
	mutex_.lock();
	waiting_ = cookie;
	while (cookie->busy_)
		done_.wait(mutex_);
	waiting_ = nullptr;
	mutex_.unlock();
	return cookie->err_;
}

int cookie::wait()
{
	if (umpi->self == &(*owner_))
		return umpi->self->wait(this);
	return err_;
}

int MPI_Wait(MPI_Request *request, MPI_Status *status)
{
	if (!umpi || !request || !*request)
		return MPI_FAIL;
	struct cookie *cookie = static_cast<struct cookie *>(*request);
	*request = nullptr;
	int err = cookie->wait();
	if (status) {
		status->MPI_SOURCE = cookie->src_;
		status->MPI_TAG = cookie->tag_;
	}
	cookie->leave();
	return err;
}

cookie *process::wait_any(int source, int tag)
{
	mutex_.lock();
	waiting_any_ = true;
	while (inbox_.empty())
		incoming_.wait(mutex_);
	box_type::iterator cookie;
	for (cookie = inbox_.begin(); !cookie->recv_match(source, tag); cookie++) {
		while (&(*cookie) == &inbox_.back())
			incoming_.wait(mutex_);
	}
	waiting_any_ = false;
	mutex_.unlock();
	return &(*cookie);
}

int MPI_Probe(int source, int tag, MPI_Comm comm, MPI_Status *status)
{
	if (!umpi || source < MPI_ANY_SOURCE || umpi->size <= source || tag < MPI_ANY_TAG || comm != MPI_COMM_WORLD || !status)
		return MPI_FAIL;
	struct cookie *cookie = umpi->self->wait_any(source, tag);
	status->MPI_SOURCE = cookie->src_;
	status->MPI_TAG = cookie->tag_;
	status->len = cookie->size_ * cookie->iovec_->len();
	return MPI_SUCCESS;
}

int MPI_Test(MPI_Request *request, int *flag, MPI_Status *status)
{
	if (!umpi || !request || !*request || !flag)
		return MPI_FAIL;
	struct cookie *cookie = static_cast<struct cookie *>(*request);
	if (cookie->busy_) {
		*flag = 0;
		return MPI_SUCCESS;
	}
	*flag = 1;
	*request = nullptr;
	int err = cookie->err_;
	if (status) {
		status->MPI_SOURCE = cookie->src_;
		status->MPI_TAG = cookie->tag_;
	}
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
	MPI_Request request;
	int ret = MPI_Iallreduce(sendbuf, recvbuf, count, datatype, op, comm, &request);
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
	int error = MPI_SUCCESS;
	if (array_of_statuses) {
		for (int i = 0; i < count; i++)
			array_of_statuses[i].MPI_ERROR = MPI_Wait(array_of_requests + i, array_of_statuses + i);
		for (int i = 0; !error && i < count; i++)
			if (array_of_statuses[i].MPI_ERROR)
				error = MPI_ERR_IN_STATUS;
	} else {
		for (int i = 0; !error && i < count; i++)
			error = MPI_Wait(array_of_requests + i, nullptr);
	}
	return error;
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

