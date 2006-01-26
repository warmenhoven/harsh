#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "main.h"

struct cookie {
	char *host;
	char *path;
	char *name;
	char *value;
	int flag;
};

static list *cookies = NULL;
static time_t mtime = 0;

static void
free_cookies()
{
	while (cookies) {
		struct cookie *cookie = cookies->data;
		cookies = list_remove(cookies, cookie);
		free(cookie->host);
		free(cookie->path);
		free(cookie->name);
		free(cookie->value);
		free(cookie);
	}
}

static void
read_cookies()
{
	struct stat buf;
	char line[8192];
	char path[1024];
	FILE *f;

	sprintf(path, "%s/.%s/cookies", getenv("HOME"), PROG);

	if (stat(path, &buf)) {
		return;
	}

	if (buf.st_mtime == mtime) {
		return;
	}

	if (!(f = fopen(path, "r"))) {
		unlink(path);
		return;
	}

	free_cookies();

	mtime = buf.st_mtime;

	while (fgets(line, 8192, f)) {
		char *host, *flag, *path, *secure, *expiration, *name, *value;
		struct cookie *cookie;

		line[strlen(line)-1] = 0;

		if (line[0] == 0 || line[0] == '#')
			continue;

		host = line;

		flag = strchr(host, '\t');
		if (!flag)
			continue;
		*flag++ = 0;

		path = strchr(flag, '\t');
		if (!path)
			continue;
		*path++ = 0;

		secure = strchr(path, '\t');
		if (!secure)
			continue;
		*secure++ = 0;

		expiration = strchr(secure, '\t');
		if (!expiration)
			continue;
		*expiration++ = 0;

		if (strcasecmp(secure, "TRUE") == 0)
			continue;

		name = strchr(expiration, '\t');
		if (!name)
			continue;
		*name++ = 0;

		value = strchr(name, '\t');
		if (!value)
			continue;
		*value++ = 0;

		cookie = malloc(sizeof (struct cookie));
		cookie->host = strdup(host);
		cookie->path = strdup(path);
		cookie->name = strdup(name);
		cookie->value = strdup(value);
		if (strcasecmp(flag, "TRUE") == 0)
			cookie->flag = 1;
		else
			cookie->flag = 0;

		cookies = list_append(cookies, cookie);
	}

	fclose(f);
}

void
pull_cookies(struct feed *feed)
{
#if 0
	char *hdrend, *databegin;

	char *cookie = NULL;

	hdrend = strstr(feed->tmpdata, "\r\n\r\n");
	if (!hdrend) {
		/* this is an error but will be picked up elsewhere */
		return;
	}

	hdrend += 2;
	*hdrend = 0;
	databegin = hdrend + 2;

	do {
		cookie = strstr(feed->tmpdata, "\r\nSet-Cookie: ");
		if (cookie) {
			char *end = strchr(cookie + 2, '\r');
			if (end) {
				*end = 0;
				feed->redir_url = strdup(location + strlen("\r\nLocation: "));
				*end = '\r';
			}
		}
	} while (cookie);

	*hdrend = '\r';
#endif
}

/* return 1 if cookies have changed, 0 otherwise */
int
find_cookies(struct feed *feed)
{
	list *l;
	char *orig = feed->cookies;
	int ret = 0;

	read_cookies();

	feed->cookies = calloc(1, 1);

	l = cookies;

	while (l) {
		struct cookie *cookie = l->data;
		l = l->next;
		char buf[1024];

		if (cookie->flag) {
			int c, f;

			c = strlen(cookie->host);
			f = strlen(feed->host);

			if (c > f)
				continue;

			if (strcasecmp(cookie->host, &feed->host[f - c]) != 0)
				continue;
		} else {
			if (strcasecmp(cookie->host, feed->host) != 0)
				continue;
		}

		if (strncasecmp(cookie->path, feed->path, strlen(cookie->path)) != 0)
			continue;

		sprintf(buf, "%s=%s; ", cookie->name, cookie->value);
		feed->cookies = realloc(feed->cookies,
								strlen(feed->cookies) + strlen(buf) + 1);
		strcat(feed->cookies, buf);
	}

	if (feed->cookies[0])
		feed->cookies[strlen(feed->cookies) - 2] = 0;

	if (orig) {
		if (strcmp(orig, feed->cookies) != 0)
			ret = 1;
	} else {
		if (feed->cookies[0] != 0)
			ret = 1;
	}

	if (orig)
		free(orig);

	return (ret);
}
