/*
umpi - intentionally small and simple MPI implementation
Written in 2014 by <Ahmet Inan> <xdsopl@googlemail.com>
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "umpi.hh"
#include <sys/wait.h>
#include <sys/mman.h>
#include <fstream>

char mmap_name[] = "/dev/shm/umpiXXXXXX";

void cleanup(int = 0)
{
	signal(SIGTERM, SIG_IGN);
	std::cerr << "terminating all remaining processes." << std::endl;
	kill(-getpid(), SIGTERM);
	std::cerr << "cleaning up." << std::endl;
	if (munmap(mmap_addr, mmap_len))
		perror("munmap");
	if (remove(mmap_name))
		perror("remove");
	exit(1);
}

int main(int argc, char **argv)
{
	if (argc < 4 || strcmp(argv[1], "-np")) {
		fprintf(stderr, "usage: %s -np N program\n", argv[0]);
		return 1;
	}
	int size = atoi(argv[2]);
	if (size <= 0 || size > 10000) {
		fprintf(stderr, "0 < N <= 10000\n");
		return 1;
	}

	int fd = mkstemp(mmap_name);
	if (fd < 0) {
		perror("mkstemp");
		return 1;
	}

	if (setenv("UMPI_MMAP", mmap_name, 1)) {
		perror("setenv");
		if (remove(mmap_name))
			perror("remove");
		return 1;
	}

	char tmp[8];
	snprintf(tmp, sizeof(tmp), "%d", size);
	if (setenv("UMPI_SIZE", tmp, 1)) {
		perror("setenv");
		if (remove(mmap_name))
			perror("remove");
		return 1;
	}

	mmap_len = calc_mmap_len(size);
	if (ftruncate(fd, mmap_len)) {
		perror("ftruncate");
		if (remove(mmap_name))
			perror("remove");
		return 1;
	}

	mmap_addr = mmap(0, mmap_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (mmap_addr == MAP_FAILED) {
		perror("mmap");
		if (remove(mmap_name))
			perror("remove");
		return 1;
	}

	if (close(fd)) {
		perror("close");
		if (munmap(mmap_addr, mmap_len))
			perror("munmap");
		if (remove(mmap_name))
			perror("remove");
		return 1;
	}

	memset(mmap_addr, 0, mmap_len);

	struct shared *shared = new (mmap_addr) struct shared(size, static_cast<uint8_t *>(mmap_addr) + sizeof(struct shared), static_cast<uint8_t *>(mmap_addr) + mmap_len);

	for (int rank = 0; rank < size; rank++) {
		pid_t pid = fork();
		if (pid < 0) {
			perror("fork");
			if (munmap(mmap_addr, mmap_len))
				perror("munmap");
			if (remove(mmap_name))
				perror("remove");
			return 1;
		} else if (0 == pid) {
			char tmp[8];
			snprintf(tmp, sizeof(tmp), "%d", rank);
			if (setenv("UMPI_RANK", tmp, 1)) {
				perror("setenv");
				if (munmap(mmap_addr, mmap_len))
					perror("munmap");
				if (remove(mmap_name))
					perror("remove");
				return 1;
			}
			std::ofstream("/proc/self/oom_score_adj") << 500 << std::endl;
			execvp(argv[3], argv+3);
			perror("execvp");
			return 1;
		}
	}

	signal(SIGTERM, cleanup);
	signal(SIGINT, cleanup);

	int status;
	pid_t pid;
	while ((pid = wait(&status)) > 0) {
		if (WIFEXITED(status) && WEXITSTATUS(status)) {
			std::cerr << "rank " << shared->find_rank(pid) << " (pid " << pid << ") terminated with nonzero exit status " << WEXITSTATUS(status) << std::endl;
			cleanup();
		} else if (WIFSIGNALED(status)) {
			std::cerr << "rank " << shared->find_rank(pid) << " (pid " << pid << ") killed by signal " << WTERMSIG(status) << " (" << strsignal(WTERMSIG(status)) << ")" << std::endl;
			cleanup();
		}
	}

	if (munmap(mmap_addr, mmap_len))
		perror("munmap");
	if (remove(mmap_name))
		perror("remove");
	return 0;
}

