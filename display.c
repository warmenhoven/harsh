#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#ifdef GOOGCORE
#include <google/coredumper.h>
#endif
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "main.h"

static int sp[2];

static enum {
	MENU,
	FEED,
	ITEM,
} mode;

static enum {
	NONE,
	ADD,
	TO,
} entry;

static char *entry_text = NULL;
static char *prompt = NULL;

static char *add_url;

static struct feed *cur_feed;
static struct item *cur_item;

static void
open_url(char *url)
{
	pid_t pid;
	char *args[4];

	pid = fork();
	if (pid != 0)
		return;

	args[0] = "screen";
	args[1] = "elinks";
	args[2] = url;
	args[3] = NULL;
	execvp(args[0], args);
}

static void
mark_item_read(struct feed *feed, struct item *item)
{
	if (item->read)
		return;
	if (item->guid)
		feed->read_guids = list_append(feed->read_guids, strdup(item->guid));
	item->read = 1;
	feed->unread--;
	save_config();
}

static void
draw_menu()
{
	list *l = feeds;
	int line = 1;
	int do_beep = 1;

	while (l) {
		struct feed *feed = l->data;
		l = l->next;
		move(line, 0);
		clrtoeol();
		if (feed == cur_feed)
			mvaddch(line, 0, '>');
		mvaddstr(line, 4, feed->title ? feed->title : feed->url);
		if (feed->fdt)
			mvaddch(line, 1, '.');
		if (feed->status != FEED_ERR_NONE) {
			mvaddch(line, 2, '!');
			move(line, 76);
			switch (feed->status) {
			case FEED_ERR_NONE:
				break;
			case FEED_ERR_URL:
				addstr("URL");
				break;
			case FEED_ERR_DNS:
				addstr("DNS");
				break;
			case FEED_ERR_NET:
				addstr("NET");
				break;
			case FEED_ERR_LIB:
				addstr("LIB");
				break;
			case FEED_ERR_SND:
				addstr("SND");
				break;
			case FEED_ERR_RCV:
				addstr("RCV");
				break;
			case FEED_ERR_HDR:
				addstr("HDR");
				break;
			case FEED_ERR_XML:
				addstr("XML");
				break;
			case FEED_ERR_RSS:
				addstr("RSS");
				break;
			}
		} else if (feed->unread)
			mvaddch(line, 2, 'N');
		if (feed->beep) {
			if (do_beep)
				beep();
			do_beep = 0;
			feed->beep = 0;
		}
		line++;
	}
}

static void
draw_feed()
{
	list *l = cur_feed->items;
	int line = 1;

	mvaddstr(line, 2, cur_feed->desc);
	line += 2;

	while (l) {
		struct item *item = l->data;
		l = l->next;
		move(line, 0);
		clrtoeol();
		if (item == cur_item)
			mvaddch(line, 0, '>');
		if (!item->read)
			mvaddch(line, 2, 'N');
		mvaddstr(line, 4, item->title ? item->title : item->desc);
		line++;
	}
}

static void
draw_item()
{
	mark_item_read(cur_feed, cur_item);
	mvaddstr(1, 2, cur_feed->desc);
	if (cur_item->title)
		mvaddstr(3, 4, cur_item->title);
	if (cur_item->desc)
		mvaddstr(5, 6, cur_item->desc);
}

static void
draw_prompt()
{
	if (entry == NONE) {
		move(LINES - 1, 0);
	} else {
		int l = strlen(prompt);
		int offset = l + 1 + strlen(entry_text) - (COLS - 1);
		if (offset <= l) {
			mvaddstr(LINES - 1, 0, offset >= 0 ? &prompt[offset] : prompt);
			addch(' ');
			addstr(entry_text);
		} else {
			offset -= l + 1;
			mvaddstr(LINES - 1, 0, &entry_text[offset]);
		}
	}
	clrtoeol();
}

static void
redraw_screen()
{
	switch (mode) {
	case MENU:
		draw_menu();
		break;
	case FEED:
		draw_feed();
		break;
	case ITEM:
		draw_item();
		break;
	}
	draw_prompt();
}

void
update_feed_display(struct feed *feed)
{
	if (mode != MENU) {
		if (cur_feed != feed)
			return;
		cur_item = feed->items ? feed->items->data : NULL;
	}
	redraw_screen();
	refresh();
}

static void
set_prompt(const char *p)
{
	if (prompt)
		free(prompt);
	prompt = strdup(p);
}

static void
set_entry(int type)
{
	entry = type;
	if (type == NONE) {
		curs_set(0);
	} else {
		curs_set(1);
		move(LINES - 1, strlen(prompt) + 1);
		clrtoeol();
	}
}

static void
ui_add_feed()
{
	set_prompt("Add feed url:");
	set_entry(ADD);
	draw_prompt();
}

static void
process_entry()
{
	static int interval;

	switch (entry) {
	case NONE:
		assert(0);
		break;
	case ADD:
		if (add_url)
			free(add_url);
		add_url = strdup(entry_text);
		*entry_text = 0;
		set_prompt("Interval in hours to check feed:");
		set_entry(TO);
		break;
	case TO:
		interval = strtoul(entry_text, NULL, 10);
		cur_feed = feed_add(add_url, interval);
		save_config();
		feed_fetch(cur_feed);
		set_entry(NONE);
		break;
	}
}

static int
entry_add(int c)
{
	int l = strlen(entry_text);

	switch (c) {
	case 7:			/* ^G */
		set_entry(NONE);
		draw_prompt();
		*entry_text = 0;
		break;
	case 8:			/* ^H */
	case 127:		/* backspace */
	case 263:		/* backspace */
		if (*entry_text) {
			entry_text[l - 1] = 0;
			draw_prompt();
		}
		break;
	case 12:		/* ^L */
		clear();
		redraw_screen();
		break;
	case 13:		/* ^M, enter */
		process_entry();
		redraw_screen();
		*entry_text = 0;
		break;
	case 21:		/* ^U */
		*entry_text = 0;
		draw_prompt();
		break;
	case 23:		/* ^W */
		c = l - 1;
		while (c > 0 && isspace(entry_text[c])) c--;
		while (c + 1 && !isspace(entry_text[c])) c--;
		c++;
		entry_text[c] = 0;
		draw_prompt();
		break;
	default:
		if (isprint(c)) {
			entry_text = realloc(entry_text, l + 2);
			entry_text[l] = c;
			entry_text[l + 1] = 0;
			draw_prompt();
		} else {
			return (0);
		}
	}
	refresh();
	return (0);
}

static void
next_unread_feed()
{
	list *l = list_find(feeds, cur_feed);
	if (!l)
		return;
	while (l->next) {
		struct feed *f;
		l = l->next;
		f = l->data;
		if (f->unread) {
			cur_feed = f;
			draw_menu();
			return;
		}
	}
	l = feeds;
	while (l->data != cur_feed) {
		struct feed *f = l->data;
		if (f->unread) {
			cur_feed = f;
			draw_menu();
			return;
		}
		l = l->next;
	}
}

static void
next_feed()
{
	list *l = list_find(feeds, cur_feed);
	if (!l || !l->next)
		return;
	cur_feed = l->next->data;
	draw_menu();
}

static void
prev_feed()
{
	list *l = list_find(feeds, cur_feed);
	if (!l || !l->prev)
		return;
	cur_feed = l->prev->data;
	draw_menu();
}

static int
menu_input(int c)
{
	if (entry != NONE) {
		return (entry_add(c));
	}

	switch (c) {
	case 9:			/* ^I, tab */
		next_unread_feed();
		break;
	case 13:		/* ^M, enter */
		mode = FEED;
		cur_item = cur_feed->items ? cur_feed->items->data : NULL;
		clear();
		redraw_screen();
		break;
	case 18:		/* ^R */
		feed_fetch(cur_feed);
		break;
	case 'a':
		ui_add_feed();
		break;
	case 'j':
		next_feed();
		break;
	case 'k':
		prev_feed();
		break;
	case 'o':
		open_url(cur_feed->link);
		break;
	case 'q':
		end_window();
		exit(0);
	default:
		return (0);
	}
	refresh();
	return (0);
}

static int
next_unread_item()
{
	list *l = list_find(cur_feed->items, cur_item);
	if (!l)
		return (0);
	while (l->next) {
		struct item *i;
		l = l->next;
		i = l->data;
		if (!i->read) {
			cur_item = i;
			return (1);
		}
	}
	l = cur_feed->items;
	while (l->data != cur_item) {
		struct item *i = l->data;
		if (!i->read) {
			cur_item = i;
			return (1);
		}
		l = l->next;
	}
	return (0);
}

static void
next_item()
{
	list *l = list_find(cur_feed->items, cur_item);
	if (!l || !l->next)
		return;
	cur_item = l->next->data;
	if (mode == FEED) {
		draw_feed();
	} else {
		clear();
		redraw_screen();
	}
}

static void
prev_item()
{
	list *l = list_find(cur_feed->items, cur_item);
	if (!l || !l->prev)
		return;
	cur_item = l->prev->data;
	if (mode == FEED) {
		draw_feed();
	} else {
		clear();
		redraw_screen();
	}
}

static int
feed_input(int c)
{
	switch (c) {
	case 9:			/* ^I, tab */
		if (next_unread_item())
			redraw_screen();
		break;
	case 13:		/* ^M, enter */
		mode = ITEM;
		clear();
		redraw_screen();
		break;
	case 18:		/* ^R */
		feed_fetch(cur_feed);
		break;
	case 'N':
		mark_item_read(cur_feed, cur_item);
		next_item();
		redraw_screen();
		break;
	case 'i':
	case 'q':
		mode = MENU;
		clear();
		redraw_screen();
		break;
	case 'j':
		next_item();
		break;
	case 'k':
		prev_item();
		break;
	case 'o':
		mark_item_read(cur_feed, cur_item);
		open_url(cur_item->link);
		redraw_screen();
		break;
	}
	refresh();
	return (0);
}

static int
item_input(int c)
{
	switch (c) {
	case 9:			/* ^I, tab */
		if (next_unread_item()) {
			clear();
			redraw_screen();
		}
		break;
	case 'i':
	case 'q':
		mode = FEED;
		clear();
		redraw_screen();
		break;
	case 'j':
		next_item();
		break;
	case 'k':
		prev_item();
		break;
	case 'o':
		open_url(cur_item->link);
		break;
	}
	refresh();
	return (0);
}

static int
stdin_ready(void *nbv, int event, nbio_fd_t *fdt)
{
	int c = getch();
#ifdef GOOGCORE
	if (c == 0xA1) {	/* M-! */
		WriteCoreDump("core.g");
		return (0);
	}
#endif
	switch (mode) {
	case MENU:
		return (menu_input(c));
	case FEED:
		return (feed_input(c));
	case ITEM:
		return (item_input(c));
	}
	return (0);
}

static int
sigwinch_redraw(void *nbv, int event, nbio_fd_t *fdt)
{
	char c;
	read(sp[0], &c, 1);
	endwin();
	initscr();
	curs_set(0);
	clear();
	redraw_screen();
	refresh();
	return (0);
}

static void
sigwinch(int sig)
{
	char c = 0;
	write(sp[1], &c, 1);
	endwin();
	initscr();
	curs_set(0);
	clear();
	redraw_screen();
	refresh();
}

static int
watch_fd(int fd, nbio_handler_t handler)
{
	nbio_fd_t *fdt;

	if (!(fdt = nbio_addfd(&gnb, NBIO_FDTYPE_STREAM, fd,
						   0, handler, NULL, 0, 0))) {
		fprintf(stderr, "Couldn't read stdin\n");
		return (1);
	}
	if (nbio_setraw(&gnb, fdt, 2)) {
		fprintf(stderr, "Couldn't read raw stdin\n");
		return (1);
	}

	return (0);
}

int
init_window()
{
	int fl;

	initscr();
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();
	curs_set(0);

	if (feeds)
		cur_feed = feeds->data;
	else
		cur_feed = NULL;

	set_entry(NONE);
	entry_text = calloc(1, 1);

	add_url = NULL;

	redraw_screen();
	refresh();

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sp))
		return (1);
	if (watch_fd(sp[0], sigwinch_redraw))
		return (1);
	if (watch_fd(0, stdin_ready))
		return (1);

	/* if we set stdin to nonblock then we need to set stdout to block */
	fl = fcntl(1, F_GETFL);
	fl ^= O_NONBLOCK;
	fcntl(1, F_SETFL, fl);

	signal(SIGWINCH, sigwinch);

	return (0);
}

void
end_window()
{
	endwin();
}
