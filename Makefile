CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L -Iinclude

TARGET = watchdog
WORKER = workers/worker

SRCS = src/main.c \
	   src/signal_manager.c \
	   src/process_manager.c \
	   src/restart_manager.c \
	   src/logger.c

OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET) $(WORKER)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(WORKER): workers/worker.c
	$(CC) $(CFLAGS) -o $@ $<

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(WORKER) watchdog.log
