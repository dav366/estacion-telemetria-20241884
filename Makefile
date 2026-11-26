CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11
SRC = src/main.c src/datos.c src/red.c src/sensor.c lib/sqlite/sqlite3.c
TARGET = build/estacion
LIBS = -lcurl -lsqlite3 -lm

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	mkdir -p build
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

run: all
	./$(TARGET)

clean:
	rm -rf build
