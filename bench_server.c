/*
 * Imperative Shell do servidor em Modo Benchmark.
 *   Gather: aceita os ClientesEsperados, lê o relógio, recebe os bytes.
 *   Decide: bench_core calcula BytesAgregados, JanelaDeMedição e TaxaAgregada.
 *   Act:    imprime a linha CSV.
 */
#include "bench_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define RECV_BUF_SIZE (256 * 1024)

typedef struct {
    int fd;
    uint64_t bytes;   /* escrito só pela thread dona da conexão */
    uint64_t end_ns;  /* instante do EOF */
} BenchConn;

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* rcvbuf é aplicado no socket de escuta para ser herdado antes do handshake
 * (o fator de window scaling é negociado no SYN). */
static int open_listener(int port, int rcvbuf) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return -1;
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (rcvbuf > 0)
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1 ||
        listen(fd, BENCH_MAX_CLIENTS) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}

/* Consome a linha do nome (handshake do protocolo do chat). */
static int skip_name_line(int fd) {
    char c = 0;
    while (c != '\n') {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n == -1 && errno == EINTR)
            continue;
        if (n <= 0)
            return -1;
    }
    return 0;
}

static int accept_clients(int lfd, BenchConn *conns, int n) {
    for (int i = 0; i < n; i++) {
        int fd = accept(lfd, NULL, NULL);
        if (fd == -1 && errno == EINTR) {
            i--;
            continue;
        }
        if (fd == -1 || skip_name_line(fd) == -1)
            return -1;
        conns[i] = (BenchConn){ fd, 0, 0 };
    }
    return 0;
}

/* Recebe e descarta até o EOF, com buffer grande (1 syscall por ~256 KiB). */
static void *drain_conn(void *arg) {
    BenchConn *c = arg;
    char *buf = malloc(RECV_BUF_SIZE);
    ssize_t n = 0;
    while (buf != NULL &&
           ((n = recv(c->fd, buf, RECV_BUF_SIZE, 0)) > 0 ||
            (n == -1 && errno == EINTR)))
        if (n > 0)
            c->bytes += (uint64_t)n;
    c->end_ns = now_ns();
    free(buf);
    return NULL;
}

static void send_start_signal(const BenchConn *conns, int n) {
    for (int i = 0; i < n; i++)
        send(conns[i].fd, "GO\n", 3, MSG_NOSIGNAL);
}

static int print_report(const BenchConn *conns, int n, uint64_t start_ns) {
    uint64_t bytes[BENCH_MAX_CLIENTS], ends[BENCH_MAX_CLIENTS];
    for (int i = 0; i < n; i++) {
        bytes[i] = conns[i].bytes;
        ends[i] = conns[i].end_ns;
    }
    uint64_t total = bench_total_bytes(bytes, (size_t)n);
    NsResult w = bench_window_ns(start_ns, ends, (size_t)n);
    MbpsResult m = bench_mbps(total, w.value);
    char line[128];
    if (w.error != BENCH_OK || m.error != BENCH_OK ||
        bench_format_report(line, sizeof(line), n, total, w.value,
                            m.value) != BENCH_OK)
        return EXIT_FAILURE;
    printf("%s\n", line);
    return EXIT_SUCCESS;
}

static int run_round(BenchConn *conns, int n) {
    pthread_t tids[BENCH_MAX_CLIENTS];
    for (int i = 0; i < n; i++)
        if (pthread_create(&tids[i], NULL, drain_conn, &conns[i]) != 0)
            return EXIT_FAILURE;
    uint64_t start_ns = now_ns();
    send_start_signal(conns, n);
    for (int i = 0; i < n; i++)
        pthread_join(tids[i], NULL);
    return print_report(conns, n, start_ns);
}

int bench_server_run(const ServerConfig *cfg) {
    static BenchConn conns[BENCH_MAX_CLIENTS];
    int n = cfg->expected_clients;
    int lfd = open_listener(cfg->port, cfg->rcvbuf);
    if (lfd == -1) {
        perror("listen");
        return EXIT_FAILURE;
    }
    fprintf(stderr, "Benchmark: aguardando %d cliente(s) na porta %d.\n",
            n, cfg->port);
    int rc = accept_clients(lfd, conns, n) == 0
                 ? run_round(conns, n) : EXIT_FAILURE;
    for (int i = 0; i < n; i++)
        if (conns[i].fd > 0)
            close(conns[i].fd);
    close(lfd);
    return rc;
}