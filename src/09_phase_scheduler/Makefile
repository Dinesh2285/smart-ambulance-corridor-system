CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/phase_scheduler

all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) scheduler_main.c

clean:
	rm -f $(TARGET)