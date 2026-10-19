CC = clang
CFLAGS = -Wall -Wextra -std=c99 -IFA_Client/includes -IFA_Server/includes -IFA_Common/includes -I/usr/local/include

LDFLAGS_CLIENT = -L/usr/local/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
LDFLAGS_SERVER = $(LDFLAGS_CLIENT)

COMMON_SRC = $(wildcard FA_Common/src/*.c)
CLIENT_SRC = $(wildcard FA_Client/src/*.c) $(COMMON_SRC)
SERVER_SRC = $(wildcard FA_Server/src/*.c) $(COMMON_SRC)

CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)

CLIENT_BIN = client
SERVER_BIN = server

.PHONY: all clean

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) -o $@ $(LDFLAGS_CLIENT)

$(SERVER_BIN): $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) -o $@ $(LDFLAGS_SERVER)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CLIENT_OBJ) $(SERVER_OBJ) $(CLIENT_BIN) $(SERVER_BIN)
