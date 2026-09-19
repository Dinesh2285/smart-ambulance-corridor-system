CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/infra_radar
all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) radar_main.c
clean:
	rm -f $(TARGET)