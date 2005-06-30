#ifndef __MAIN_H__
#define __MAIN_H__

#define HAVE_SYS_SOCKET_H
#include <libnbio.h>
#include <stdint.h>
#include "list.h"

#define PROG "harsh"
#define VERS "0.01"
#define URL  "http://www.warmenhoven.org/src/"
#define EMAIL "eric@warmenhoven.org"

#define USER_AGENT PROG "/" VERS " (" URL "; " EMAIL ")"

enum feed_status {
	FEED_ERR_NONE,
	FEED_ERR_URL,	/* problem parsing URL */
	FEED_ERR_DNS,	/* problem resolving host */
	FEED_ERR_NET,	/* problem connecting to server */
	FEED_ERR_LIB,	/* problem inside nbio */
	FEED_ERR_SND,	/* problem writing request */
	FEED_ERR_RCV,	/* problem receiving response */
	FEED_ERR_HDR,	/* problem with HTTP headers */
	FEED_ERR_XML,	/* problem with XML data */
	FEED_ERR_RSS,	/* problem with the RSS data */
};

struct feed {
	char *url;
	char *cookies;
	char *modified;

	enum feed_status status;

	char *user;
	char *pass;
	char *host;
	int port;
	char *path;

	nbio_fd_t *fdt;

	char *data;
	int datalen;
	char *tmpdata;
	int tmpdatalen;

	uint32_t interval;
	time_t next_poll;
};

extern list *feeds;
extern nbio_t gnb;

extern int read_config(void);
extern int save_config(void);

extern int read_cookies(void);
extern int find_cookies(struct feed *);

extern int init_window(void);
extern void update_feed_display(struct feed *);
extern void end_window(void);

extern int init_feeds(void);
extern struct feed *feed_add(char *, uint32_t);
extern void feed_fetch(struct feed *);

extern int feed_delay();
extern void feed_poll();

extern void rss_parse(struct feed *, void *);

#endif
