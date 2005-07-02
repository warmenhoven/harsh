#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "main.h"

int
read_config()
{
	struct stat sb;
	char path[1024];

	sprintf(path, "%s/.%s", getenv("HOME"), PROG);
	if (stat(path, &sb))
		mkdir(path, 0700);
	else if (!S_ISDIR(sb.st_mode)) {
		unlink(path);
		mkdir(path, 0700);
	}

	sprintf(path, "%s/.%s/config", getenv("HOME"), PROG);
	if (stat(path, &sb))
		return (0);
	else if (!S_ISREG(sb.st_mode)) {
		unlink(path);
		return (0);
	} else {
		FILE *f = fopen(path, "r");
		char line[8192];
		if (!f) {
			unlink(path);
			return (0);
		}
		while (fgets(line, 8192, f)) {
			uint32_t interval;
			char *url;

			struct feed *feed;

			line[strlen(line)-1] = 0;

			if (line[0] == '#')
				continue;

			interval = strtoul(line, NULL, 10);
			url = strchr(line, ' ') + 1;

			feed = feed_add(url, interval);
		}
		fclose(f);
	}

	return (0);
}

int
save_config()
{
	list *l = feeds;
	FILE *f = NULL;
	char path[256];

	sprintf(path, "%s/.%s/config", getenv("HOME"), PROG);
	if (!(f = fopen(path, "w"))) {
		return (1);
	}

	while (l) {
		struct feed *feed = l->data;
		l = l->next;
		fprintf(f, "%u %s\n", feed->interval, feed->url);
	}

	fclose(f);

	return (0);
}
