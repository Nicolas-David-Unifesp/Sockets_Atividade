#ifndef BENCH_CLIENT_H
#define BENCH_CLIENT_H

#include "bench_core.h"

/* Imperative Shell do ClienteBenchmark. Retorna EXIT_SUCCESS/EXIT_FAILURE. */
int bench_client_run(const ClientConfig *cfg);

#endif