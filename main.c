#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "main.h"

nbio_t gnb;

void
LOG(char *fmt, ...)
{
#ifdef EST
	va_list ap;
	time_t t;
	struct tm *tmp;
	char tmstr[20];

	static FILE *logfile = NULL;

	if (!logfile) {
		char path[1024];
		sprintf(path, "%s/log", mydir());
		logfile = fopen(path, "w+");
		if (!logfile) {
			return;
		}
	}

	t = time(NULL);
	tmp = localtime(&t);
	strftime(tmstr, sizeof (tmstr), "%m/%d %X", tmp);
	fprintf(logfile, "%s ", tmstr);

	va_start(ap, fmt);
	vfprintf(logfile, fmt, ap);
	va_end(ap);

	fprintf(logfile, "\n");
#endif
}

char *
mydir()
{
	struct stat sb;
	static int init = 0;
	static char path[1024];
	char env[1024];
	int i;

	if (init)
		return path;

	sprintf(env, "%sDIR", PROG);
	for (i = 0; i < strlen(PROG); i++)
		env[i] = toupper(env[i]);

	if (getenv(env)) {
		sprintf(path, "%s", getenv(env));
	} else {
		sprintf(path, "%s/.%s", getenv("HOME"), PROG);
	}

	/* make sure the directory exists and is a directory */
	if (stat(path, &sb))
		mkdir(path, 0700);
	else if (!S_ISDIR(sb.st_mode)) {
		unlink(path);
		mkdir(path, 0700);
	}

	init = 1;

	return path;
}

int
main()
{
	if (read_config())
		return (1);

	if (nbio_init(&gnb, 100)) {
		fprintf(stderr, "Couldn't init IO\n");
		return (1);
	}

	if (init_window())
		return (1);

	if (init_feeds())
		return (1);

	while (1) {
		int ms_delay = feed_delay();
		/*
		 * depending on what HZ is in the kernel, poll(2) may not be able to
		 * handle a very large timeout. but 30 min should work on anybody's
		 * system.
		 */
		ms_delay = ms_delay < 30 * 60 * 1000 ? ms_delay : 30 * 60 * 1000;
		if (nbio_poll(&gnb, ms_delay) == -1)
			break;
		waitpid(-1, NULL, WNOHANG);
		feed_poll();
	}

	end_window();

	return (0);
}
