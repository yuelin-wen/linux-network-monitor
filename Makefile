CC=g++
CFLAGS=-I
CFLAGS+=-Wall
FILES1=intfMonitor.cpp
FILES2=networkMonitor.cpp

interface1: $(FILES1)
	$(CC) $(CFLAGS) $^ -o $@

interface2: $(FILES1)
	$(CC) $(CFLAGS) $^ -o $@


netMonitor: $(FILES2)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f *.o interface1 interface2 netMonitor

all: interface1 interface2 netMonitor

