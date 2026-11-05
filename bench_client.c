/*
 * Imperative Shell do ClienteBenchmark.
 *   Gather: conecta, aguarda o SinalDeInício, lê o relógio.
 *   Act:    envia BlocosDeEnvio até o fim da DuraçãoDeEnvio.
 *   Decide: bench_core calcula a taxa vista pelo cliente (informativa, em stderr).
 */
#include "bench_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* Opções aplicadas antes do connect, para valerem já no handshake. */
static void apply_socket_options(int fd, const ClientConfig *cfg) {
    if (cfg->sndbuf > 0)
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &cfg->sndbuf, sizeof(cfg->sndbuf));
    if (cfg->nodelay)
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &cfg->nodelay, sizeof(cfg->nodelay));
}

static int connect_to_server(const ClientConfig *cfg) {
    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)cfg->port);
    if (inet_pton(AF_INET, cfg->ip, &addr.sin_addr) != 1)
        return -1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;
    apply_socket_options(fd, cfg);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Envia o nome (protocolo do chat) e espera a linha "GO". */
static int handshake(int fd) {
    char line[16];
    int len = snprintf(line, sizeof(line), "bench%d\n", (int)getpid() % 100000);
    if (send(fd, line, (size_t)len, MSG_NOSIGNAL) != len)
        return -1;
    size_t pos = 0;
    while (pos < sizeof(line) - 1) {
        ssize_t n = recv(fd, &line[pos], 1, 0);
        if (n == -1 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;
        if (line[pos++] == '\n')
            break;
    }
    line[pos] = '\0';
    return strcmp(line, "GO\n") == 0 ? 0 : -1;
}

/* Envia blocos sem parar até o prazo. Envios parciais apenas somam bytes:
 * o conteúdo é irrelevante e não há enquadramento no Modo Benchmark. */
static uint64_t send_until(int fd, const char *block, size_t size,
                           uint64_t deadline_ns) {
    uint64_t sent = 0;
    while (now_ns() < deadline_ns) {
        ssize_t n = send(fd, block, size, MSG_NOSIGNAL);
        if (n > 0)
            sent += (uint64_t)n;
        else if (!(n == -1 && errno == EINTR))
            break;
    }
    return sent;
}

static void report_client(uint64_t sent, uint64_t elapsed_ns) {
    MbpsResult m = bench_mbps(sent, elapsed_ns);
    if (m.error == BENCH_OK)
        fprintf(stderr, "cliente %d: %llu bytes enviados, %.2f Mbps\n",
                (int)getpid(), (unsigned long long)sent, m.value);
}

int bench_client_run(const ClientConfig *cfg) {
    char *block = malloc(cfg->block_size);
    int fd = connect_to_server(cfg);
    if (block == NULL || fd == -1 || handshake(fd) == -1) {
        perror("bench client");
        free(block);
        if (fd != -1)
            close(fd);
        return EXIT_FAILURE;
    }
    memset(block, 'x', cfg->block_size);
    uint64_t start = now_ns();
    uint64_t sent = send_until(fd, block, cfg->block_size,
                               start + (uint64_t)cfg->duration_s * 1000000000ull);
    uint64_t elapsed = now_ns() - start;
    shutdown(fd, SHUT_WR);
    close(fd);
    free(block);
    report_client(sent, elapsed);
    return EXIT_SUCCESS;
}