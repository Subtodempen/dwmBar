CC = g++
CFLAGS = -lxcb

all: dwmBar debug

dwmBar: main.cpp
	$(CC) $(CFLAGS) main.cpp -o dwmBar

debug: main.cpp
	$(CC) $(CFLAGS) -g main.cpp -o dwmBarDebug

clean:
	rm -f dwmBar dwmBarDebug
