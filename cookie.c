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

	sprintf(path, "%s/cookies", mydir());

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
		if (*host == '.')
			host++;
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

static void
pull_cookie(struct feed *feed, char *string)
{
	char *name, *value, *end;
	struct cookie *cookie;

	name = string + strlen("Set-Cookie: ");

	value = strchr(name, '=');
	if (!value)
		return;
	*value++ = 0;

	end = strchr(value, ';');
	if (end)
		*end = '\0';

	/* yes i should parse host and path, no i don't care that i don't */

	if (strlen(value) != 0) {
		LOG("feed %s adding cookie %s as %s",
			feed->redir_url ? feed->redir_url : feed->url, name, value);

		cookie = malloc(sizeof (struct cookie));
		cookie->host = NULL;
		cookie->path = NULL;
		cookie->name = strdup(name);
		cookie->value = strdup(value);
		cookie->flag = 0;
		feed->setcookies = list_append(feed->setcookies, cookie);
	}

	*(--value) = '=';
	if (end)
		*end = ';';
}

void
pull_cookies(struct feed *feed)
{
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

	cookie = feed->tmpdata;
	do {
		cookie = strstr(cookie, "\r\nSet-Cookie: ");
		if (cookie) {
			char *end = strchr(cookie + 2, '\r');
			if (end) {
				*end = 0;
				pull_cookie(feed, cookie + 2);
				*end = '\r';
			}
			cookie = end;
		}
	} while (cookie);

	*hdrend = '\r';
}

static void
walk_cookies(struct feed *feed, list *l, int check_skip)
{
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
		} else if (cookie->host) {
			if (strcasecmp(cookie->host, feed->host) != 0)
				continue;
		}

		if (cookie->path) {
			if (strncasecmp(cookie->path, feed->path, strlen(cookie->path)) != 0)
				continue;
		}

		if (check_skip) {
			/* go through all of the setcookies, see if this cookie has been
			 * overwritten */
			list *s = feed->setcookies;
			while (s) {
				struct cookie *sc = s->data;
				if (strcasecmp(cookie->name, sc->name) == 0)
					break;
				s = s->next;
			}
			if (s)
				continue;
		}

		sprintf(buf, "%s=%s; ", cookie->name, cookie->value);
		feed->cookies = realloc(feed->cookies,
								strlen(feed->cookies) + strlen(buf) + 1);
		strcat(feed->cookies, buf);
	}
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

	l = feed->setcookies;

	walk_cookies(feed, feed->setcookies, 0);
	walk_cookies(feed, cookies, 1);

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
