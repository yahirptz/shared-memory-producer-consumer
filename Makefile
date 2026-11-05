CC = cc
CFLAGS = -std=c11 -O0 -Wall -Wextra -Wpedantic

.PHONY: all clean run
all: a.out producer consumer

a.out: main.c shared.h
	$(CC) $(CFLAGS) main.c -o a.out

producer: producer.c shared.h
	$(CC) $(CFLAGS) producer.c -o producer

consumer: consumer.c shared.h
	$(CC) $(CFLAGS) consumer.c -o consumer

run: all
	./a.out 8 2 2 30

clean:
	rm -f a.out producer consumer
