TARGET = harsh
LDLIBS = /usr/lib/libnbio.a -lcurses -lexpat /usr/local/lib/libcoredumper.a
CFLAGS = -g3 -O3 -Wall -Werror -I/usr/include/libnbio

SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = $(patsubst %.c, %.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

$(OBJS): $(HDRS)

# don't run this close to midnight
dist:
	rm -rf tmp
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)-`date +%Y%m%d`
	cp Makefile SConstruct TODO $(SRCS) $(HDRS) tmp/$(TARGET)-`date +%Y%m%d`
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)-`date +%Y%m%d`
	rm -rf tmp

.PHONY: dist
