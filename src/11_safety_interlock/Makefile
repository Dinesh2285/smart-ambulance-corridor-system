CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/safety_interlock

all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) interlock_main.c

clean:
	rm -f $(TARGET)