#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "main.h"

static char *
fix_path(char *url)
{
	char *fix = strdup(url);
	char *tmp = fix;

	while (*tmp) {
		if (*tmp == '/')
			*tmp = '_';
		tmp++;
	}

	return (fix);
}

static int
read_feed(struct feed *feed)
{
	struct stat sb;
	char path[1024], *fix;

	fix = fix_path(feed->url);
	sprintf(path, "%s/.%s/%s", getenv("HOME"), PROG, fix);
	free(fix);

	if (stat(path, &sb))
		return (0);
	else if (!S_ISREG(sb.st_mode)) {
		unlink(path);
		return (0);
	} else {
		FILE *f = fopen(path, "r");

		if (!f) {
			unlink(path);
			return (0);
		}

		feed->tmpdatalen = sb.st_size;
		feed->tmpdata = malloc(feed->tmpdatalen);

		fread(feed->tmpdata, feed->tmpdatalen, 1, f);

		fclose(f);
	}

	return (0);
}

int
save_feed(struct feed *feed)
{
	FILE *f = NULL;
	char path[256], *fix;

	fix = fix_path(feed->url);
	sprintf(path, "%s/.%s/%s", getenv("HOME"), PROG, fix);
	free(fix);
	if (!(f = fopen(path, "w"))) {
		return (1);
	}

	fwrite(feed->data, feed->datalen, 1, f);

	return (0);
}

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
			uint32_t refresh;
			char *url;

			struct feed *feed;

			line[strlen(line)-1] = 0;

			refresh = strtoul(line, NULL, 10);
			url = strchr(line, ' ') + 1;

			feed = feed_add(url, refresh);

			read_feed(feed);
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
		fprintf(f, "%u %s\n", feed->refresh, feed->url);
	}

	fclose(f);

	return (0);
}
