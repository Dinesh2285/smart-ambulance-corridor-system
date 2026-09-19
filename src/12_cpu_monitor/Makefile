CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/cpu_monitor

all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) cpu_monitor_main.c

clean:
	rm -f $(TARGET)