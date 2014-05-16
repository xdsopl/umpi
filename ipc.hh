/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#ifndef IPC_HH
#define IPC_HH

#include <pthread.h>
#include <stdio.h>
#include <cstdlib>

namespace ipc {
	class mutex {
		friend class condition_variable;
	public:
		mutex()
		{
			pthread_mutexattr_t mutexattr;
			if (pthread_mutexattr_init(&mutexattr)) {
				perror("pthread_mutexattr_init");
				std::exit(1);
			}
			if (pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED)) {
				perror("pthread_mutexattr_setshared");
				std::exit(1);
			}
			if (pthread_mutex_init(&mutex_, &mutexattr)) {
				perror("pthread_mutex_init");
				std::exit(1);
			}
		}
		void lock() { pthread_mutex_lock(&mutex_); }
		void unlock() noexcept { pthread_mutex_unlock(&mutex_); }
	private:
		pthread_mutex_t mutex_;
	};

	class condition_variable {
	public:
		condition_variable()
		{
			pthread_condattr_t condattr;
			if (pthread_condattr_init(&condattr)) {
				perror("pthread_condattr_init");
				std::exit(1);
			}
			if (pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED)) {
				perror("pthread_condattr_setshared");
				std::exit(1);
			}
			if (pthread_cond_init(&cond_, &condattr)) {
				perror("pthread_cond_init");
				std::exit(1);
			}
		}
		void notify_one() noexcept { pthread_cond_signal(&cond_); }
		void notify_all() noexcept { pthread_cond_broadcast(&cond_); }
		void wait(mutex &m) { pthread_cond_wait(&cond_, &m.mutex_); }
	private:
		pthread_cond_t cond_;
	};

	class barrier {
	public:
		barrier(int count)
		{
			pthread_barrierattr_t barrierattr;
			if (pthread_barrierattr_init(&barrierattr)) {
				perror("pthread_barrierattr_init");
				std::exit(1);
			}

			if (pthread_barrierattr_setpshared(&barrierattr, PTHREAD_PROCESS_SHARED)) {
				perror("pthread_barrierattr_setshared");
				std::exit(1);
			}

			if (pthread_barrier_init(&barrier_, &barrierattr, count)) {
				perror("pthread_barrier_init");
				std::exit(1);
			}
		}
		void wait() { pthread_barrier_wait(&barrier_); }
	private:
		pthread_barrier_t barrier_;
	};
}

#endif
