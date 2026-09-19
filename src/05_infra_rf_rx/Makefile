CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/infra_rf_rx
all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) rf_rx_main.c
clean:
	rm -f $(TARGET)