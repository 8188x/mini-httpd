CC      = cc
CFLAGS  = -Wall -Wextra -O2 -std=c99 -pedantic -Wno-gnu-zero-variadic-macro-arguments
LDFLAGS =

SRCDIR  = src
SRCS    = $(SRCDIR)/main.c $(SRCDIR)/server.c $(SRCDIR)/http.c \
          $(SRCDIR)/static.c $(SRCDIR)/mime.c
OBJS    = $(SRCS:.c=.o)
TARGET  = mini-httpd

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET) -p 8080 -r www
