TARGET=hw2
OBJ=queue.o scheduler.o thread.o testcase.o TestCase1.o TestCase2.o TestCase3.o
CC=gcc
CFLAGS=-c

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $(TARGET) $(OBJ)

.c.o:
	$(CC) $(CFLAGS) $<

clean:
	rm -rf $(OBJ)
