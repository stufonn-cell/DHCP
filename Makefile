# Makefile for DHCP Server and Client

CC = cc
CFLAGS = -O2
SERVER_SRC = server.c
CLIENT_SRC = client.c
RELAY_SRC = relayDhcp.c
SERVER_BIN = server.out
CLIENT_BIN = client.out
RELAY_BIN = relay.out

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)

server:
	clear
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC) -pthread
	sudo ./$(SERVER_BIN)

client:
	clear
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC)
	sudo ./$(CLIENT_BIN)

relay:
	clear
	$(CC) $(CFLAGS) -o $(RELAY_BIN) $(RELAY_SRC)
	sudo ./$(RELAY_BIN) $(ip)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)

.PHONY: all clean