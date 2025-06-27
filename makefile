CC = gcc
CFLAGS = -Wall -O2
SRC = main.c lodepng.c
OBJ = $(SRC:.c=.o)
TARGET = segment

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all clean
