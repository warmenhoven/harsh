#ifndef __MAIN_H__
#define __MAIN_H__

#define HAVE_SYS_SOCKET_H
#include <libnbio.h>
#include <stdint.h>
#include "list.h"

#define PROG "harsh"

enum feed_status {
	FEED_ERR_NONE,
	FEED_ERR_URL,
	FEED_ERR_DNS,
	FEED_ERR_NET,
	FEED_ERR_LIB,
	FEED_ERR_SND,
	FEED_ERR_RCV,
	FEED_ERR_RSS,
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

	uint32_t refresh;
};

extern list *feeds;
extern nbio_t gnb;

extern int read_config(void);
extern int save_config(void);
extern int save_feed(struct feed *);

extern int read_cookies(void);
extern int find_cookies(struct feed *);

extern int init_window(void);
extern void end_window(void);

extern int init_feeds(void);
extern struct feed *feed_add(char *, uint32_t);
extern void feed_fetch(struct feed *);

#endif
