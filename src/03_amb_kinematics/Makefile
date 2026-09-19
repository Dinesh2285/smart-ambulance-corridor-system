CC = qcc
CFLAGS = -Vgcc_ntoaarch64le -Wall -O2
TARGET = ../../Binaries/amb_kinematics
all:
	mkdir -p ../../Binaries
	$(CC) $(CFLAGS) -o $(TARGET) dead_reckon_main.c -lm
clean:
	rm -f $(TARGET)