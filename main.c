#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include "main.h"

nbio_t gnb;

int
main()
{
	if (read_cookies())
		return (1);

	if (read_config())
		return (1);

	if (nbio_init(&gnb, 15)) {
		fprintf(stderr, "Couldn't init IO\n");
		return (1);
	}

	if (init_window())
		return (1);

	if (init_feeds())
		return (1);

	while (1) {
		int ms_delay = feed_delay();
		if (nbio_poll(&gnb, ms_delay) == -1)
			break;
		waitpid(-1, NULL, WNOHANG);
		feed_poll();
	}

	end_window();

	return (0);
}
