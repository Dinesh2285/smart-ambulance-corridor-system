CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/cli_dashboard

all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) console_main.c

clean:
	rm -f $(TARGET)