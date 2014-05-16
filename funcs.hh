/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef FUNCS_HH
#define FUNCS_HH

struct umpi_op
{
	umpi_op(MPI_User_function *f) : func(f) {}
	void operator()(const void *invec, void *inoutvec, int *len, MPI_Datatype *datatype) const
	{
		func(const_cast<void *>(invec), inoutvec, len, datatype);
	}
private:
	MPI_User_function *func;
};

template<template<class> class Op>
struct umpi_generic_op: public umpi_op
{
	constexpr umpi_generic_op() : umpi_op(function) {}
private:
	static void function(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype)
	{
		switch ((*datatype)->id()) {
			case UMPI_ID_CONTIGUOUS:
			case UMPI_ID_SCATTERED:
			case UMPI_ID_PACKED:
			case UMPI_ID_BYTE:
				std::cerr << "Invalid datatype used in predefined MPI_Op." << std::endl;
				std::exit(1);
			case UMPI_ID_CHAR:
				return apply(static_cast<const char *>(invec), static_cast<char *>(inoutvec), len);
			case UMPI_ID_UNSIGNED_CHAR:
				return apply(static_cast<const unsigned char *>(invec), static_cast<unsigned char *>(inoutvec), len);
			case UMPI_ID_SHORT:
				return apply(static_cast<const short *>(invec), static_cast<short *>(inoutvec), len);
			case UMPI_ID_UNSIGNED_SHORT:
				return apply(static_cast<const unsigned short *>(invec), static_cast<unsigned short *>(inoutvec), len);
			case UMPI_ID_INT:
				return apply(static_cast<const int *>(invec), static_cast<int *>(inoutvec), len);
			case UMPI_ID_UNSIGNED:
				return apply(static_cast<const unsigned *>(invec), static_cast<unsigned *>(inoutvec), len);
			case UMPI_ID_LONG:
				return apply(static_cast<const long *>(invec), static_cast<long *>(inoutvec), len);
			case UMPI_ID_UNSIGNED_LONG:
				return apply(static_cast<const unsigned long *>(invec), static_cast<unsigned long *>(inoutvec), len);
			case UMPI_ID_FLOAT:
				return apply(static_cast<const float *>(invec), static_cast<float *>(inoutvec), len);
			case UMPI_ID_DOUBLE:
				return apply(static_cast<const double *>(invec), static_cast<double *>(inoutvec), len);
			case UMPI_ID_LONG_DOUBLE:
				return apply(static_cast<const long double *>(invec), static_cast<long double *>(inoutvec), len);
		}
	}
	template<class T>
	static void apply(const T *invec, T *inoutvec, int *len)
	{
		Op<T> op;
		for (int i = 0; i < *len; i++)
			inoutvec[i] = op(invec[i], inoutvec[i]);
	}
};

template<class T>
struct umpi_max
{
	T operator()(const T &a, const T &b) const { return std::max(a, b); }
};

template<class T>
struct umpi_min
{
	T operator()(const T &a, const T &b) const { return std::min(a, b); }
};

#endif
