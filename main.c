#include <stdio.h>
#include <string.h>
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
		if (nbio_poll(&gnb, -1) == -1)
			break;
	}

	end_window();

	return (0);
}
