# Makefile, ECE252
# Yiqing Huang, 2018/11/02

CC = gcc
CFLAGS = -Wall -std=c99 -g # "curl-config --cflags" output is empty
LD = gcc
LDFLAGS = -std=c99 -g
LDLIBS = -lcurl # "curl-config --libs" output

SRCS   = paster-single.c paster-multi.c
OBJS1  = paster-single.o
OBJS2  = paster-multi.o
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
