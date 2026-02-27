CC = gcc
CFLAGS = -Wall -Wextra -Werror -Iinclude
COMMON_SRC = src/common/process.c src/common/config.c src/common/logging.c
DAEMON_SRC = src/daemon/main.c $(COMMON_SRC)
CLIENT_SRC = src/client/main.c
DAEMON_NAME = taskmasterd
CLIENT_NAME = taskmasterctl

all: $(DAEMON_NAME) $(CLIENT_NAME)

$(DAEMON_NAME): $(DAEMON_SRC)
	$(CC) $(CFLAGS) -o $@ $^

$(CLIENT_NAME): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f src/common/*.o src/daemon/*.o src/client/*.o

fclean: clean
	rm -f $(DAEMON_NAME) $(CLIENT_NAME)

re: fclean all

test: all
	./tests/run_tests.sh

.PHONY: all clean fclean re test
