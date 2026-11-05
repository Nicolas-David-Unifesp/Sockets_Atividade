#include "bench_core.h"

#include <stdio.h>
#include <string.h>

uint64_t bench_total_bytes(const uint64_t *per_client, size_t n) {
    uint64_t total = 0;
    for (size_t i = 0; i < n; i++)
        total += per_client[i];
    return total;
}

NsResult bench_window_ns(uint64_t start_ns, const uint64_t *end_ns, size_t n) {
    NsResult r = { BENCH_ERR_USAGE, 0 };
    if (n == 0)
        return r;

    uint64_t last = end_ns[0];
    for (size_t i = 0; i < n; i++) {
        if (end_ns[i] < start_ns) {
            r.error = BENCH_ERR_ZERO_TIME;
            return r;
        }
        if (end_ns[i] > last)
            last = end_ns[i];
    }
    r.error = BENCH_OK;
    r.value = last - start_ns;
    return r;
}

MbpsResult bench_mbps(uint64_t bytes, uint64_t elapsed_ns) {
    MbpsResult r = { BENCH_ERR_ZERO_TIME, 0.0 };
    if (elapsed_ns == 0)
        return r;
    /* bits / segundo / 1e6  ==  bytes * 8 * 1e9 / ns / 1e6  ==  bytes * 8000 / ns */
    r.error = BENCH_OK;
    r.value = (double)bytes * 8000.0 / (double)elapsed_ns;
    return r;
}

BenchError bench_format_report(char *out, size_t cap, int clients,
                               uint64_t bytes, uint64_t elapsed_ns,
                               double mbps) {
    int n = snprintf(out, cap, "%d,%llu,%.3f,%.2f", clients,
                     (unsigned long long)bytes,
                     (double)elapsed_ns / 1e9, mbps);
    if (n < 0 || (size_t)n >= cap)
        return BENCH_ERR_BUFFER;
    return BENCH_OK;
}

/* ---- Parse de argumentos ---- */

/* Aceita apenas dígitos decimais; max pequeno evita overflow. */
LongResult bench_parse_long(const char *text, long min, long max) {
    LongResult r = { BENCH_ERR_RANGE, 0 };
    if (text == NULL || *text == '\0')
        return r;

    long v = 0;
    for (const char *p = text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9')
            return r;
        v = v * 10 + (*p - '0');
        if (v > max)
            return r;
    }
    if (v < min)
        return r;
    r.error = BENCH_OK;
    r.value = v;
    return r;
}

/* Argumento posicional opcional: usa def quando ausente. */
static LongResult optional_long(int argc, char *const argv[], int idx,
                                long min, long max, long def) {
    if (idx >= argc) {
        LongResult r = { BENCH_OK, def };
        return r;
    }
    return bench_parse_long(argv[idx], min, max);
}

static BenchError first_error(const LongResult *rs, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (rs[i].error != BENCH_OK)
            return rs[i].error;
    return BENCH_OK;
}

static int is_bench_flag(int argc, char *const argv[], int idx) {
    return argc > idx && strcmp(argv[idx], "--bench") == 0;
}

/* <porta> | <porta> --bench <N> [rcvbuf] */
ServerConfigResult bench_parse_server_args(int argc, char *const argv[]) {
    ServerConfigResult r = { BENCH_ERR_USAGE, { 0, 0, 0, 0 } };
    int bench = argc >= 4 && argc <= 5;
    if (argc != 2 && !bench)
        return r;
    if (bench && !is_bench_flag(argc, argv, 2))
        return r;

    LongResult v[3] = {
        bench_parse_long(argv[1], 1, 65535),
        optional_long(argc, argv, 3, 1, BENCH_MAX_CLIENTS, 0),
        optional_long(argc, argv, 4, 0, BENCH_MAX_SOCKBUF, 0),
    };
    r.error = first_error(v, 3);
    if (r.error == BENCH_OK)
        r.value = (ServerConfig){ (int)v[0].value, bench,
                                  (int)v[1].value, (int)v[2].value };
    return r;
}

/* <IP> <porta> | <IP> <porta> --bench <seg> [bloco] [nodelay] [sndbuf] */
ClientConfigResult bench_parse_client_args(int argc, char *const argv[]) {
    ClientConfigResult r = { BENCH_ERR_USAGE, { NULL, 0, 0, 0, 0, 0, 0 } };
    int bench = argc >= 5 && argc <= 8;
    if (argc != 3 && !bench)
        return r;
    if (bench && !is_bench_flag(argc, argv, 3))
        return r;

    LongResult v[5] = {
        bench_parse_long(argv[2], 1, 65535),
        optional_long(argc, argv, 4, 1, BENCH_MAX_DURATION_S, 0),
        optional_long(argc, argv, 5, 1, BENCH_MAX_BLOCK, BENCH_DEFAULT_BLOCK),
        optional_long(argc, argv, 6, 0, 1, 0),
        optional_long(argc, argv, 7, 0, BENCH_MAX_SOCKBUF, 0),
    };
    r.error = first_error(v, 5);
    if (r.error == BENCH_OK)
        r.value = (ClientConfig){ argv[1], (int)v[0].value, bench,
                                  (int)v[1].value, (size_t)v[2].value,
                                  (int)v[3].value, (int)v[4].value };
    return r;
}