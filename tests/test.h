/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef TEST_H
#define TEST_H

#include <time.h>
#include <errno.h>

void fsleep(double seconds)
{
	struct timespec ts;
	ts.tv_sec = seconds;
	ts.tv_nsec = (seconds - ts.tv_sec) * 1000000000.0;
	errno = 0;
	while (nanosleep(&ts, &ts) && errno == EINTR);
}

#endif

