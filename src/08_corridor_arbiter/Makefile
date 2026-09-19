CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/corridor_arbiter

# Compile both .c files together
all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) arbiter_main.c priority_queue.c

clean:
	rm -f $(TARGET)