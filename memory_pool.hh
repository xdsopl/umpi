/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef MEMORY_POOL_HH
#define MEMORY_POOL_HH

#include <mutex>
#include <iostream>

template<class P, size_t max_dealloc_size, class M = std::mutex>
class memory_pool {
	typedef P pointer;
	typedef M mutex_type;
	typedef typename std::pointer_traits<pointer> pointer_traits;
	typedef typename pointer_traits::template rebind<pointer> pointer_pointer;
	typedef typename pointer_traits::template rebind<uint8_t> byte_pointer;
	typedef typename pointer_traits::template rebind<void> void_pointer;
	static const size_t align_size = alignof(std::max_align_t);
	static const size_t list_size = (max_dealloc_size + align_size - 1) / align_size;
public:
	memory_pool(void *begin, void *end) :
		first_(), count_(),
		space_(static_cast<uint8_t *>(end) - static_cast<uint8_t *>(begin)),
		begin_(std::align(align_size, align(sizeof(pointer_pointer)), begin, space_)) {}

	pointer alloc(size_t len)
	{
		if (!len)
			return nullptr;
		size_t aligned_len = align(std::max(sizeof(pointer_pointer), len));
		size_t i = list(aligned_len);
		mutex_.lock();
		//std::cerr << "alloc: " << len << " bytes, giving: " << aligned_len << " bytes." << std::endl;
		if (i < list_size && !empty(i)) {
			pointer tmp(pop(i));
			mutex_.unlock();
			return tmp;
		}
		if (aligned_len > space_) {
			mutex_.unlock();
			std::cerr << "no more pool memory." << std::endl;
			std::exit(1);
		}
		byte_pointer tmp(begin_);
		begin_ = tmp + aligned_len;
		space_ -= aligned_len;
		mutex_.unlock();
		return tmp;
	}

	void dealloc(pointer p, size_t len)
	{
		if (!len)
			return;
		size_t aligned_len = align(std::max(sizeof(pointer_pointer), len));
		size_t i = list(aligned_len);
		mutex_.lock();
		//std::cerr << "dealloc: " << len << " bytes, taking: " << aligned_len << " bytes." << std::endl;
		if (i >= list_size) {
			mutex_.unlock();
			std::cerr << "deallocation of only size <= " << align(max_dealloc_size) << " allowed." << std::endl;
			std::exit(1);
		}
		push(i, p);
		mutex_.unlock();
	}

	size_t max_size(size_t len)
	{
		if (!len)
			return size_t(-1);
		size_t aligned_len = align(std::max(sizeof(pointer_pointer), len));
		size_t i = list(aligned_len);
		return std::min(space_ / aligned_len, i < list_size && count_[i] ? 1 : 0);
	}
private:
	void push(size_t i, pointer p) {
		count_[i]++;
		pointer_pointer tmp(p);
		*tmp = first_[i];
		first_[i] = tmp;
	}

	pointer pop(size_t i) {
		count_[i]--;
		pointer tmp(first_[i]);
		first_[i] = *first_[i];
		return tmp;
	}

	bool empty(size_t i) const { return !count_[i]; }
	size_t align(size_t v) const { return (v + align_size - 1) & ~(align_size - 1); }
	size_t list(size_t v) const { return (v - 1) / align_size; }

	mutex_type mutex_;
	pointer_pointer first_[list_size];
	size_t count_[list_size];
	size_t space_;
	void_pointer begin_;
};

#endif
