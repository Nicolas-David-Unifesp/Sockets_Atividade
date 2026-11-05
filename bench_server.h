#ifndef BENCH_SERVER_H
#define BENCH_SERVER_H

#include "bench_core.h"

/* Imperative Shell do servidor em Modo Benchmark. Retorna EXIT_SUCCESS/EXIT_FAILURE. */
int bench_server_run(const ServerConfig *cfg);

#endif