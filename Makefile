TOPDIR = /usr
TARGET = harsh
LDLIBS = $(TOPDIR)/lib/libnbio.a -lcurses -lexpat $(TOPDIR)/lib/libcoredumper.a
CFLAGS = -g3 -O3 -Wall -Werror -I$(TOPDIR)/include/libnbio

SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = $(patsubst %.c, %.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDLIBS)

$(OBJS): $(HDRS)

clean:
	@rm -f $(OBJS) $(TARGET) core core.*

# don't run this close to midnight
dist:
	rm -rf tmp
	rm -f $(TARGET).tgz
	mkdir -p tmp/$(TARGET)-`date +%Y%m%d`
	cp Makefile SConstruct TODO $(SRCS) $(HDRS) tmp/$(TARGET)-`date +%Y%m%d`
	cd tmp && tar zcf ../$(TARGET).tgz $(TARGET)-`date +%Y%m%d`
	rm -rf tmp

.PHONY: dist
