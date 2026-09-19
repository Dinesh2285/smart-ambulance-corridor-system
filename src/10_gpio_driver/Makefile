CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/gpio_driver

all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) gpio_main.c

clean:
	rm -f $(TARGET)