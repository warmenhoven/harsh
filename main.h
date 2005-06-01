#ifndef __MAIN_H__
#define __MAIN_H__

#include <expat.h>
#define HAVE_SYS_SOCKET_H
#include <libnbio.h>
#include <stdint.h>
#include "list.h"

#define PROG "harsh"

struct feed {
	char *url;
	char *data;
	uint32_t refresh;
};

extern list *feeds;
extern nbio_t gnb;

extern int read_config(void);

extern int init_window(void);
extern void end_window(void);

extern int init_feeds(void);
extern void feed_add(const char *);

#endif

/* vim:set sw=4 ts=4 noet ai cin tw=80: */
