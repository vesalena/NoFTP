.PHONY: all run clean

all: server client

run:
	./server &
	./client &

server: server.cpp packet.h
	g++ -std=c++11 -fsanitize=address server.cpp -o server

client: client.cpp packet.h
	g++ -std=c++11 -fsanitize=address client.cpp -o client

clean:
	rm -rf server
	rm -rf client

