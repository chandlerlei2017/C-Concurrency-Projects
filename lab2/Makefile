# Makefile, ECE252
# Yiqing Huang, 2018/11/02

CC = gcc
CFLAGS = -Wall -std=c99 -g # "curl-config --cflags" output is empty
LD = gcc
LDFLAGS = -std=c99 -g
LDLIBS = -lcurl -lz # "curl-config --libs" output

LIB_UTIL = zutil.o crc.o

SRCS   = paster-single.c paster-multi.c crc.c zutil.c
OBJS1  = paster-single.o $(LIB_UTIL)
OBJS2  = paster-multi.o $(LIB_UTIL)
TARGETS= paster-single paster-multi

all: ${TARGETS}

paster-single: $(OBJS1)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

paster-multi: $(OBJS2)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o $(TARGETS) *.png
