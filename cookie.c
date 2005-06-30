#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int
find_cookies(struct feed *feed)
{
	list *l = cookies;
	char *orig = feed->cookies;
	int ret = 0;

	feed->cookies = calloc(1, 1);

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

int
read_cookies()
{
	char line[8192];
	char path[1024];
	FILE *f;

	sprintf(path, "%s/.%s/cookies", getenv("HOME"), PROG);

	if (!(f = fopen(path, "r"))) {
		unlink(path);
		return (0);
	}

	free_cookies();

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

	return (0);
}
