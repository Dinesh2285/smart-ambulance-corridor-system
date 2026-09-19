CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2

TARGET = ../../Binaries/supervisor
SRC = watchdog_main.c

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)