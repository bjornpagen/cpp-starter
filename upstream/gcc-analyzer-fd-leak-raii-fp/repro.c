/* Plain-C form of the caller-owned-fd false positive (no C++ involved).
   gcc-16 -O2 -fanalyzer -c repro.c */
#include <sys/socket.h>
#include <unistd.h>

struct server {
	int listener;
};

int accept_one(struct server *s)
{
	int fd = accept(s->listener, 0, 0);
	if (fd < 0)
		return -1;
	close(fd);
	return 0;
}
