CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/amb_rf_tx
all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) rf_tx_main.c
clean:
	rm -f $(TARGET)