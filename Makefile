CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=gnu11
LDLIBS  = -pthread

.PHONY: all test test-integration report clean

all: server client

server: server_chat.c bench_server.c bench_core.c bench_core.h bench_server.h
	$(CC) $(CFLAGS) -o $@ server_chat.c bench_server.c bench_core.c $(LDLIBS)

client: client_chat.c bench_client.c bench_core.c bench_core.h bench_client.h
	$(CC) $(CFLAGS) -o $@ client_chat.c bench_client.c bench_core.c $(LDLIBS)

tests/test_bench_core: tests/test_bench_core.c bench_core.c bench_core.h
	$(CC) $(CFLAGS) -o $@ tests/test_bench_core.c bench_core.c -lm

test: tests/test_bench_core
	./tests/test_bench_core

test-integration: all
	python3 tests/test_bench_integration.py

report:
	python3 scripts/gerar_relatorio.py

clean:
	rm -f server client tests/test_bench_core
