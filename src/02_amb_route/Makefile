CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/amb_route
all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) route_main.c
clean:
	rm -f $(TARGET)