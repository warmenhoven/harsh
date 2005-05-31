#include <curses.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include "main.h"

static int sp[2];

static void
redraw_screen()
{
    refresh();
}

static int
stdin_ready(void *nbv, int event, nbio_fd_t *fdt)
{
	int c = getch();
    switch (c) {
    case 'q':
        end_window();
        exit(0);
        break;
    }
	refresh();
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

	redraw_screen();

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

/* vim:set sw=4 ts=4 et ai cin tw=80: */
