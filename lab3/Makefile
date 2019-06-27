# Makefile, ECE252
# Yiqing Huang, 2018/11/02

CC = gcc
CFLAGS = -Wall -g # "curl-config --cflags" output is empty
LD = gcc
LDFLAGS = -g
LDLIBS = -lcurl -lz -pthread# "curl-config --libs" output

LIB_UTIL = zutil.o crc.o

SRCS   = paster2.c crc.c zutil.c
OBJS2  = paster2.o $(LIB_UTIL)
TARGETS= paster2

all: ${TARGETS}

paster2: $(OBJS2)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) *.png
