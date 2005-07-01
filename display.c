#include <assert.h>
#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "main.h"

static int sp[2];

static enum {
	MENU,
	FEED,
} mode;

static enum {
	NONE,
	ADD,
	TO,
} entry;

static char *entry_text = NULL;
static char *prompt = NULL;

static char *add_url;

static void
draw_menu()
{
	list *l = feeds;
	int line = 0;

	while (l) {
		struct feed *feed = l->data;
		l = l->next;
		move(line, 0);
		clrtoeol();
		mvaddstr(line, 4, feed->title ? feed->title : feed->url);
		if (feed->fdt)
			mvaddch(line, 1, '.');
		if (feed->status != FEED_ERR_NONE)
			mvaddch(line, 2, '!');
		line++;
	}
}

static void
draw_feed()
{
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
	}
	draw_prompt();
}

void
update_feed_display(struct feed *feed)
{
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
	struct feed *feed;
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
		feed = feed_add(add_url, interval);
		save_config();
		feed_fetch(feed);
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

static int
menu_input(int c)
{
	if (entry != NONE) {
		return (entry_add(c));
	}

	switch (c) {
	case 'a':
		ui_add_feed();
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
feed_input(int c)
{
	switch (c) {
	}
	return (0);
}

static int
stdin_ready(void *nbv, int event, nbio_fd_t *fdt)
{
	int c = getch();
	switch (mode) {
	case MENU:
		return (menu_input(c));
	case FEED:
		return (feed_input(c));
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
