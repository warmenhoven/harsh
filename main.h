#ifndef __MAIN_H__
#define __MAIN_H__

#define HAVE_SYS_SOCKET_H
#include <libnbio.h>
#include <stdint.h>
#include "list.h"

#ifdef EST
#define PROG "harshest"
#else
#define PROG "harsh"
#endif
#define VERS "43"
#define URL  "http://www.warmenhoven.org/src/"
#define EMAIL "eric@warmenhoven.org"

#define USER_AGENT PROG "/" VERS " (" URL "; " EMAIL ")"

#define MAX_REDIR	10

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
	char *redir_url;
	char *cookies;
	char *modified;

	enum feed_status status;
	int redir_count;

	list *setcookies;

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

	char *title;
	char *link;
	char *desc;
	list *items;

	list *read_guids;
	int unread;
	int beep;
};

struct item {
	char *guid;
	time_t time;
	char *title;
	char *link;
	char *desc;
	char *creator;
	int read;
};

extern list *feeds;
extern nbio_t gnb;

extern void __attribute__((__format__(__printf__, 1, 2))) LOG(char *, ...);

extern char *mydir(void);

extern int read_config(void);
extern int save_config(void);

extern void pull_cookies(struct feed *);
extern int find_cookies(struct feed *);

extern int init_window(void);
extern void update_feed_display(struct feed *);
extern void end_window(void);

extern int init_feeds(void);
extern struct feed *feed_add(char *, uint32_t);
extern struct feed *feed_del(struct feed *);
extern void feed_fetch(struct feed *);

extern int feed_delay();
extern void feed_poll();

extern void rss_parse(struct feed *, void *);

#endif
