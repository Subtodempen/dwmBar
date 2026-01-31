CC = g++
CFLAGS = -lxcb -Wall -Wextra
DEBUGFLAGS = -g -O0

all: dwmBar debug

dwmBar: main.cpp
	$(CC) $(CFLAGS) main.cpp -o dwmBar

debug: main.cpp
	$(CC) $(CFLAGS) $(DEBUGFLAGS) main.cpp -o dwmBarDebug

clean:
	rm -f dwmBar dwmBarDebug
