CC = gcc       # compiler
CFLAGS = -Wall # compilation flg
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz   # link with libz

# For students
LIB_UTIL = zutil.o crc.o
SRCS   = cat_png.c crc.c zutil.c
OBJS   = cat_png.o $(LIB_UTIL)
OBJS2  = find_png.o

TARGETS= catpng findpng

all: ${TARGETS}

catpng: $(OBJS)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

findpng: $(OBJS2)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS)
