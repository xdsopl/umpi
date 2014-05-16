/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef POOL_ALLOCATOR_HH
#define POOL_ALLOCATOR_HH

#include <utility>
#include <memory>

template<class P, class M, M **pool, class T = typename P::element_type>
class pool_allocator {
	typedef typename std::pointer_traits<P> pointer_traits;
public:
	typedef P pointer;
	typedef T value_type;
	typedef value_type &reference;
	typedef const value_type &const_reference;
	typedef typename pointer_traits::template rebind<const value_type> const_pointer;
	typedef typename pointer_traits::template rebind<void> void_pointer;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;

	template<class U>
	struct rebind {
		typedef pool_allocator<typename pointer_traits::template rebind<U>, M, pool> other;
	};

	pool_allocator() noexcept {}
	pool_allocator(const pool_allocator &) noexcept {}

	template<class U>
	pool_allocator(const pool_allocator<typename pointer_traits::template rebind<U>, M, pool> &) noexcept
	{}

	static pointer address(reference x) noexcept { return pointer(&x); }
	static const_pointer address(const_reference x) noexcept { return const_pointer(&x); }

	static pointer allocate(size_type n, void_pointer = nullptr)
	{
		return (*pool)->alloc(n * sizeof(value_type));
	}

	static void deallocate(pointer p, size_type n) noexcept
	{
		(*pool)->dealloc(p, n * sizeof(value_type));
	}

	static size_type max_size() noexcept
	{
		return (*pool)->max_size(sizeof(value_type));
	}

	template<class... Args>
	static void construct(pointer p, Args&&... args)
	{
		new (static_cast<void *>(p)) value_type(std::forward<Args>(args)...);
	}
	static void destroy(pointer p) { p->~value_type(); }
};

template<class P, class M, M **pool>
class pool_allocator<P, M, pool, void> {
	typedef typename std::pointer_traits<P> pointer_traits;
public:
	template<class U>
	struct rebind {
		typedef pool_allocator<typename pointer_traits::template rebind<U>, M, pool> other;
	};
};

#endif
