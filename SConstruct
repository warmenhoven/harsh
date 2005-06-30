# vim:set filetype=python:

prog = Environment(CCFLAGS	= '-g3 -O3 -Wall -Werror',
		   CPPPATH	= ['/usr/include/libnbio'])

prog.CacheDir('cache')

conf = Configure(prog)

if not conf.CheckLibWithHeader('nbio',
							   ['sys/socket.h', 'libnbio.h'],
							   'c', 'nbio_init(NULL, 0);'):
	print 'nbio not found'
	Exit(1)

if not conf.CheckLibWithHeader('ncurses', 'curses.h', 'c',
			       'initscr();'):
	print 'ncurses not found'
	Exit(1)

slist = [
		 'config.c',
		 'cookie.c',
		 'display.c',
		 'feed.c',
		 'list.c',
		 'main.c',
		 'rss.c',
		 'xml.c'
		]

if not conf.CheckLibWithHeader('expat', 'expat.h', 'c',
				       'XML_ParserCreate(0);'):
	print 'expat not found'
	Exit(1)

harsh = prog.Program(target = 'harsh', source = slist)

conf.Finish()

Default(harsh)
