/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef BASED_PTR_HH
#define BASED_PTR_HH

#include <cstddef>
#include <cstdint>
#include <utility>
#include <memory>

template<class T, void **addr>
class based_ptr {
	template<class, void **> friend class based_ptr;
	explicit based_ptr(ptrdiff_t ptr) : ptr_(ptr) {}
	static const ptrdiff_t nullptr_ = std::numeric_limits<ptrdiff_t>::min();
public:
	typedef T element_type;

	based_ptr(std::nullptr_t = nullptr)
	: ptr_(nullptr_)
	{}

	based_ptr(element_type *ptr)
	: ptr_(!ptr ? nullptr_ : reinterpret_cast<uint8_t *>(ptr) - static_cast<uint8_t *>(*addr))
	{}

	template<class U>
	based_ptr(const based_ptr<U, addr> &other)
	: ptr_(other.ptr_)
	{}

	explicit operator bool() const { return ptr_ != nullptr_; }

	bool operator==(const based_ptr &other) const { return ptr_ == other.ptr_; }
	bool operator!=(const based_ptr &other) const { return ptr_ != other.ptr_; }
	bool operator<=(const based_ptr &other) const { return ptr_ <= other.ptr_; }
	bool operator>=(const based_ptr &other) const { return ptr_ >= other.ptr_; }
	bool operator<(const based_ptr &other) const { return ptr_ < other.ptr_; }
	bool operator>(const based_ptr &other) const { return ptr_ > other.ptr_; }

	element_type &operator*() const { return *reinterpret_cast<element_type *>(static_cast<uint8_t *>(*addr) + ptr_); }
	element_type *operator->() const { return reinterpret_cast<element_type *>(static_cast<uint8_t *>(*addr) + ptr_); }
	explicit operator void *() const { return static_cast<uint8_t *>(*addr) + ptr_; }
	explicit operator element_type *() const { return reinterpret_cast<element_type *>(static_cast<uint8_t *>(*addr) + ptr_); }

	based_ptr &operator++() { ptr_ += sizeof(element_type); return *this; }
	based_ptr &operator--() { ptr_ -= sizeof(element_type); return *this; }

	based_ptr operator++(int) { based_ptr copy(*this); ++(*this); return copy; }
	based_ptr operator--(int) { based_ptr copy(*this); --(*this); return copy; }

	based_ptr &operator+=(ptrdiff_t i) { ptr_ += sizeof(element_type) * i; return *this; }
	based_ptr &operator-=(ptrdiff_t i) { ptr_ -= sizeof(element_type) * i; return *this; }

	based_ptr operator+(ptrdiff_t i) const { return based_ptr(ptr_ + sizeof(element_type) * i); }
	based_ptr operator-(ptrdiff_t i) const { return based_ptr(ptr_ - sizeof(element_type) * i); }

	ptrdiff_t operator-(const based_ptr<const element_type, addr> &other) const { return (ptr_ - other.ptr_) / sizeof(element_type); }

	element_type &operator[](size_t i) const { return *(*this + ptrdiff_t(i)); }

	static based_ptr pointer_to(element_type &element) { return based_ptr(&element); }
private:
	ptrdiff_t ptr_;
};

template<void **addr>
class based_ptr<void, addr> {
	template<class, void **> friend class based_ptr;
	explicit based_ptr(ptrdiff_t ptr) : ptr_(ptr) {}
	static const ptrdiff_t nullptr_ = std::numeric_limits<ptrdiff_t>::min();
public:
	typedef void element_type;

	based_ptr(std::nullptr_t = nullptr)
	: ptr_(nullptr_)
	{}

	based_ptr(void *ptr)
	: ptr_(!ptr ? nullptr_ : static_cast<uint8_t *>(ptr) - static_cast<uint8_t *>(*addr))
	{}

	template<class U>
	based_ptr(const based_ptr<U, addr> &other)
	: ptr_(other.ptr_)
	{}

	explicit operator bool() const { return ptr_ != nullptr_; }

	bool operator==(const based_ptr &other) const { return ptr_ == other.ptr_; }
	bool operator!=(const based_ptr &other) const { return ptr_ != other.ptr_; }

	void *operator->() const { return static_cast<uint8_t *>(*addr) + ptr_; }
	explicit operator void *() const { return static_cast<uint8_t *>(*addr) + ptr_; }
private:
	ptrdiff_t ptr_;
};

namespace std {
	template<class T, void **addr>
	struct pointer_traits<based_ptr<T, addr>> {
		typedef T element_type;
		typedef based_ptr<element_type, addr> pointer;
		typedef ptrdiff_t difference_type;
		template <class U> using rebind = based_ptr<U, addr>;
		static pointer pointer_to(element_type &element) { return pointer(&element); }
	};

	template<void **addr>
	struct pointer_traits<based_ptr<void, addr>> {
		typedef void element_type;
		typedef based_ptr<element_type, addr> pointer;
		typedef ptrdiff_t difference_type;
		template <class U> using rebind = based_ptr<U, addr>;
	};

	template<void **addr>
	struct pointer_traits<based_ptr<const void, addr>> {
		typedef const void element_type;
		typedef based_ptr<element_type, addr> pointer;
		typedef ptrdiff_t difference_type;
		template <class U> using rebind = based_ptr<U, addr>;
	};
}

#endif
