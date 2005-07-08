TARGET = harsh
LDLIBS = -lnbio -lcurses -lexpat
CFLAGS = -g3 -O3 -Wall -Werror

SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
OBJS = $(patsubst %.c, %.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)

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
