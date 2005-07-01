TARGET = harsh
LDLIBS = -lnbio -lcurses -lexpat
CFLAGS = -g3 -O3 -Wall -Werror

OBJS = config.o cookie.o display.o feed.o list.o main.o rss.o xml.o

all: $(TARGET)

$(TARGET): $(OBJS)

$(OBJS): list.h main.h xml.h
