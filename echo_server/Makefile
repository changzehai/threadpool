all: server client

server: thread_pool.c
	gcc -W -Wall -o server echo_server.c thread_pool.c -lpthread -I.
client: simple_client.c
	gcc -W -Wall -o $@ $<
clean:
	rm server client
