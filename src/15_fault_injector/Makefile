CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/chaos_injector

all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) chaos_main.c

clean:
	rm -f $(TARGET)