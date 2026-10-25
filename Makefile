CC = gcc
UNAME_S := $(shell uname -s)

# Flags de base communs
CFLAGS = -Wall -Wextra -std=c99 -IFA_Client/includes -IFA_Server/includes -IFA_Common/includes
LDFLAGS_SERVER = -lm

# Détection automatique : macOS (Darwin) vs Linux (Fedora / Ubuntu VPS)
ifeq ($(UNAME_S), Darwin)
    BREW_PREFIX := $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)
    CFLAGS += -I$(BREW_PREFIX)/include
    LDFLAGS_CLIENT = -L$(BREW_PREFIX)/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -lm
else
    LDFLAGS_CLIENT = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

COMMON_SRC = $(wildcard FA_Common/src/*.c)
CLIENT_SRC = $(wildcard FA_Client/src/*.c) $(COMMON_SRC)
SERVER_SRC = FA_Server/src/main.c FA_Common/src/map.c FA_Common/src/drone.c FA_Common/src/weaponSlot.c

CLIENT_OBJ = $(CLIENT_SRC:.c=.o)

CLIENT_BIN = client
SERVER_BIN = server

.PHONY: all clean

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) -o $@ $(LDFLAGS_CLIENT)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -DSERVER_BUILD $(SERVER_SRC) -o $@ $(LDFLAGS_SERVER)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CLIENT_OBJ) $(CLIENT_BIN) $(SERVER_BIN)