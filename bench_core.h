/*
 * Functional Core do Modo Benchmark.
 * Somente funções puras: sem I/O, sem relógio, sem alocação.
 * Erros são retornados em tipos Result explícitos.
 */

 
#ifndef BENCH_CORE_H
#define BENCH_CORE_H

#include <stddef.h>
#include <stdint.h>

#define BENCH_MAX_CLIENTS    100
#define BENCH_MAX_DURATION_S 3600
#define BENCH_MAX_BLOCK      (16u * 1024u * 1024u)
#define BENCH_MAX_SOCKBUF    (64 * 1024 * 1024)
#define BENCH_DEFAULT_BLOCK  (128u * 1024u)

typedef enum {
    BENCH_OK = 0,
    BENCH_ERR_USAGE,     /* número/forma de argumentos inválidos */
    BENCH_ERR_RANGE,     /* valor numérico inválido ou fora do limite */
    BENCH_ERR_ZERO_TIME, /* janela de medição nula ou negativa */
    BENCH_ERR_BUFFER     /* buffer de saída insuficiente */
} BenchError;

typedef struct { BenchError error; long value; } LongResult;
typedef struct { BenchError error; double value; } MbpsResult;
typedef struct { BenchError error; uint64_t value; } NsResult;

typedef struct {
    int port;
    int bench;            /* 1 = Modo Benchmark */
    int expected_clients;
    int rcvbuf;           /* 0 = autotuning do kernel */
} ServerConfig;
typedef struct { BenchError error; ServerConfig value; } ServerConfigResult;

typedef struct {
    const char *ip;
    int port;
    int bench;
    int duration_s;
    size_t block_size;
    int nodelay;          /* 1 = TCP_NODELAY ligado */
    int sndbuf;           /* 0 = autotuning do kernel */
} ClientConfig;
typedef struct { BenchError error; ClientConfig value; } ClientConfigResult;

LongResult bench_parse_long(const char *text, long min, long max);

ServerConfigResult bench_parse_server_args(int argc, char *const argv[]);
ClientConfigResult bench_parse_client_args(int argc, char *const argv[]);

uint64_t   bench_total_bytes(const uint64_t *per_client, size_t n);
NsResult   bench_window_ns(uint64_t start_ns, const uint64_t *end_ns, size_t n);
MbpsResult bench_mbps(uint64_t bytes, uint64_t elapsed_ns);

BenchError bench_format_report(char *out, size_t cap, int clients,
                               uint64_t bytes, uint64_t elapsed_ns,
                               double mbps);

#endif