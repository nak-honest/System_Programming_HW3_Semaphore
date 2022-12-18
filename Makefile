TARGET=hw2
OBJ=queue.o scheduler.o thread.o semaphore.o testcase.o
CC=gcc
CFLAGS=-c

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ)

.c.o:
	$(CC) $(CFLAGS) $<

clean:
	rm -rf $(OBJ)
