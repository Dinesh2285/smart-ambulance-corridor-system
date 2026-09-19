CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/logic_analyzer

all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) analyzer_main.c

clean:
	rm -f $(TARGET)