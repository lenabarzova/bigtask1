CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11
LIBS = -lm
SRC = main.c lodepng.c
OBJ = $(SRC:.c=.o)
TARGET = segment

all: $(TARGET)

$(TARGET): $(OBJ)
  $(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
  $(CC) $(CFLAGS) -c $< -o $@

clean:
  rm -f $(OBJ) $(TARGET) *.png

.PHONY: all clean
