#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "xml.h"

list *feeds = NULL;

static void
feed_parse(struct feed *feed)
{
	char *hdrend, *databegin;

	char *lastmod;

	void *xml_tree;

	hdrend = strstr(feed->data, "\r\n\r\n");
	assert(hdrend);

	hdrend += 2;
	*hdrend = 0;
	databegin = hdrend + 2;

	lastmod = strstr(feed->data, "\r\nLast-Modified: ");
	if (lastmod) {
		char *end = strchr(lastmod + 2, '\r');
		if (end) {
			*end = 0;
			if (feed->modified)
				free(feed->modified);
			feed->modified = strdup(lastmod + strlen("\r\nLast-Modified: "));
			*end = '\r';
		}
	}

	/*
	 * we might get Set-Cookie: here but we ignore it. i don't consider that a
	 * bug. if you want fresh cookies then you should bake them yourself.
	 */

	xml_tree = xml_parse(databegin, strlen(databegin));
	if (xml_tree == NULL) {
		feed->status = FEED_ERR_XML;
		return;
	}

	rss_parse(feed, xml_tree);

	xml_free(xml_tree);
}

static void
feed_check(struct feed *feed)
{
	char *hdrend;
	int code;

	if (feed->tmpdata == NULL || feed->tmpdatalen == 0) {
		return;
	}

	hdrend = strstr(feed->tmpdata, "\r\n\r\n");
	if (!hdrend) {
		feed->status = FEED_ERR_HDR;
		return;
	}

	if (strncasecmp(feed->tmpdata, "HTTP/", 5) != 0) {
		feed->status = FEED_ERR_HDR;
		return;
	}

	code = strtoul(feed->tmpdata + 9, NULL, 10);

	if (code >= 500) {
		feed->status = FEED_ERR_HDR;
		return;
	} else if (code >= 400) {
		feed->status = FEED_ERR_HDR;
		return;
	} else if (code >= 300) {
		if (code == 304) {
			/*
			 * the contents have not been modified. we don't need to update
			 * feed->status because at the start of feed_fetch() we set the
			 * status to FEED_ERR_NONE.
			 */
		} else {
			/* XXX need to handle redirects, etc. */
		}
	} else if (code >= 200) {
		if (feed->data)
			free(feed->data);
		feed->data = feed->tmpdata;
		feed->datalen = feed->tmpdatalen;
		feed->tmpdata = NULL;
		feed->tmpdatalen = 0;
		feed_parse(feed);
	} else {
		feed->status = FEED_ERR_HDR;
		return;
	}
}

static void
feed_close(struct feed *feed)
{
	nbio_closefdt(&gnb, feed->fdt);
	feed->fdt = NULL;
	update_feed_display(feed);
}

static int
feed_callback(void *nb, int event, nbio_fd_t *fdt)
{
	struct feed *feed = fdt->priv;

	if (event == NBIO_EVENT_READ) {
		char buf[1024];
		int len;

		if ((len = recv(fdt->fd, buf, sizeof (buf) - 1, 0)) < 0) {
			feed->status = FEED_ERR_RCV;
			feed_close(feed);
			return (0);
		}

		if (len == 0) {
			feed->tmpdata = realloc(feed->tmpdata, feed->tmpdatalen + 1);
			feed->tmpdata[feed->tmpdatalen] = 0;
			feed_check(feed);
			feed_close(feed);
			return (0);
		}

		feed->tmpdata = realloc(feed->tmpdata, feed->tmpdatalen + len);
		memcpy(&feed->tmpdata[feed->tmpdatalen], buf, len);
		feed->tmpdatalen += len;

		return (0);
	} else if (event == NBIO_EVENT_WRITE) {
		unsigned char *buf;
		int offset, len;

		if (!(buf = nbio_remtoptxvector(nb, fdt, &len, &offset))) {
			feed->status = FEED_ERR_SND;
			feed_close(feed);
			return (0);
		}

		free(buf);

		return (0);
	} else if ((event == NBIO_EVENT_ERROR) || (event == NBIO_EVENT_EOF)) {
		feed->status = FEED_ERR_NET;
		feed_close(feed);
		return (0);
	}

	return (-1);
}

static void
send_request(struct feed *feed)
{
	char *req;
	int len;

	len = strlen("GET ") + strlen(feed->path) + strlen(" HTTP/1.0\r\n");
	len += strlen("Host: ") + strlen(feed->host) + strlen("\r\n");
	len += strlen("User-Agent: ") + strlen(USER_AGENT) + strlen("\r\n");
	if (feed->cookies) {
		len += strlen("Cookie: ") + strlen(feed->cookies) + strlen("\r\n");
	}
	if (feed->modified) {
		len += strlen("If-Modified-Since: ") + strlen(feed->modified) +
			strlen("\r\n");
	}
	if (feed->user && feed->pass) {
		len += strlen("Authorization: Basic ") +
			strlen(feed->user) + strlen(":") + strlen(feed->pass) +
			strlen("\r\n");
	}
	len += strlen("\r\n") + 1;
	req = malloc(len);

	sprintf(req, "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: %s\r\n",
			feed->path, feed->host, USER_AGENT);
	if (feed->cookies) {
		strcat(req, "Cookie: ");
		strcat(req, feed->cookies);
		strcat(req, "\r\n");
	}
	if (feed->modified) {
		strcat(req, "If-Modified-Since: ");
		strcat(req, feed->modified);
		strcat(req, "\r\n");
	}
	if (feed->user && feed->pass) {
		strcat(req, "Authorization: Basic ");
		strcat(req, feed->user);
		strcat(req, ":");
		strcat(req, feed->pass);
		strcat(req, "\r\n");
	}
	strcat(req, "\r\n");

	if (nbio_addtxvector(&gnb, feed->fdt, req, len) == -1) {
		free(req);
		feed->status = FEED_ERR_SND;
		feed_close(feed);
		return;
	}

	if (feed->tmpdata)
		free(feed->tmpdata);
	feed->tmpdata = calloc(1, 1);
	feed->tmpdatalen = 0;
}

static int
feed_connected(void *nb, int event, nbio_fd_t *fdt)
{
	struct feed *feed = fdt->priv;

	if (feed->fdt) {
		/*
		 * we're already connected, we must have tried to connect twice, ignore
		 * this one, because another one is already successful
		 */
		nbio_closefdt(nb, fdt);
		return (0);
	}

	if (event == NBIO_EVENT_CONNECTED) {
		if (!(fdt = nbio_addfd(nb, NBIO_FDTYPE_STREAM, fdt->fd,
							   0, feed_callback, feed, 0, 128))) {
			feed->status = FEED_ERR_LIB;
			return (0);
		}

		feed->fdt = fdt;
		update_feed_display(feed);
		nbio_setraw(nb, fdt, 2);

		send_request(feed);
	} else if (event == NBIO_EVENT_CONNECTFAILED) {
		nbio_closefdt(nb, fdt);
		feed->status = FEED_ERR_NET;
	}

	return (0);
}

static char *
base64enc(char *pass)
{
	static const char *enc =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	char *ret, *c, *p;
	int sl, len;

	sl = strlen(pass);
	len = ((sl * 4) / 3) + 1;
	ret = calloc(1, len);
	c = ret;

	p = pass;

	while (sl >= 3) {
		*c++ = enc[p[0] >> 2];
		*c++ = enc[((p[0] << 4) & 0x30) | ((p[1] >> 4) & 0x0f)];
		*c++ = enc[((p[1] << 2) & 0x3c) | ((p[2] >> 6) & 0x03)];
		*c++ = enc[p[2] & 0x3f];
		p += 3;
		sl -= 3;
	}

	if (sl) {
		*c++ = enc[p[0] >> 2];
		if (sl == 1) {
			*c++ = enc[((p[0] << 4) & 0x30)];
			*c++ = '=';
			*c++ = '=';
		} else {
			*c++ = enc[((p[0] << 4) & 0x30) | ((p[1] >> 4) & 0x0f)];
			*c++ = enc[((p[1] << 2) & 0x3c)];
			*c++ = '=';
		}
	}

	*c++ = 0;

	return (ret);
}

static int
parse_url(struct feed *feed)
{
	char *url = feed->url;
	char *path, *tmp;

	if (strncasecmp(url, "feed:", 5) == 0) {
		url += 5;
	}

	if (strncasecmp(url, "http://", 7) == 0) {
		url += 7;
	} else if (strncasecmp(url, "rss://", 6) == 0) {
		url += 6;
	} else if (strstr(url, "://")) {
		feed->status = FEED_ERR_URL;
		return (1);
	}

	path = strchr(url, '/');
	if (!path) {
		feed->status = FEED_ERR_URL;
		return (1);
	}
	*path = 0;

	tmp = strchr(url, '@');
	if (tmp) {
		char *at = tmp;
		*at = 0;
		tmp = strchr(url, ':');
		if (tmp) {
			*tmp = 0;
			feed->user = strdup(url);
			feed->pass = base64enc(tmp + 1);
			*tmp = ':';
		} else {
			feed->user = strdup(url);
		}
		*at = '@';
		url = at + 1;
	}

	tmp = strchr(url, ':');
	if (tmp) {
		*tmp = 0;
		feed->host = strdup(url);
		feed->port = strtol(tmp + 1, NULL, 10);
		if (feed->port <= 0 || feed->port > 0xffff) {
			feed->status = FEED_ERR_URL;
			*tmp = ':';
			*path = '/';
			return (1);
		}
		*tmp = ':';
	} else {
		feed->host = strdup(url);
		feed->port = 80;
	}

	*path = '/';
	feed->path = strdup(path);

	return (0);
}

void
feed_fetch(struct feed *feed)
{
	struct sockaddr_in sa;
	struct hostent *hp;

	if (feed->fdt) {
		/*
		 * we're already connected and we're trying to connect again. what do
		 * you think, should we kill the current connection and start a new one?
		 * i don't think so. it's easier to just ignore this request.
		 */
		return;
	}

	feed->status = FEED_ERR_NONE;

	if (parse_url(feed))
		return;
	if (find_cookies(feed)) {
		/* if the cookies have changed, invalidate modified-since. */
		if (feed->modified)
			free(feed->modified);
		feed->modified = NULL;
	}

	if (!(hp = gethostbyname(feed->host))) {
		feed->status = FEED_ERR_DNS;
		return;
	}

	memset(&sa, 0, sizeof (struct sockaddr_in));
	sa.sin_port = htons(feed->port);
	memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
	sa.sin_family = hp->h_addrtype;

	if (nbio_connect(&gnb, (struct sockaddr *)&sa, sizeof (sa),
					 feed_connected, feed)) {
		feed->status = FEED_ERR_NET;
	}
}

struct feed *
feed_add(char *url, uint32_t refresh)
{
	struct feed *nf = calloc(1, sizeof (struct feed));
	nf->url = strdup(url);
	nf->refresh = refresh;

	feeds = list_append(feeds, nf);

	return (nf);
}

int
init_feeds()
{
	list *l = feeds;

	while (l) {
		struct feed *feed = l->data;
		l = l->next;
		feed_fetch(feed);
	}

	return (0);
}
