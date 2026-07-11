CC = cc
CFLAGS = -O2 -Wall -Wextra -std=c11 -Wno-format-truncation \
          $(addprefix -I,$(shell find src -type d))

SRC = $(shell find src -name "*.c")
OBJ = $(SRC:.c=.o)

BIN = minippl

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: clean