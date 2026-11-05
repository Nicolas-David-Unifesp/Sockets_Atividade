# Especificação: Modo Benchmark 

## O que foi feito?
- Medir a TaxaAgregada recebida pelo servidor com 1 a 10 ClientesBenchmark simultâneos.
- Otimizar o caminho de dados (cliente → servidor) para a vazão máxima.

## O que ainda não foi feito?
- Gráficos e relatório analítico. (Essa análise precisa ser feita, pois a tabela pode ser efetuada, mas os gráficos ainda não)

## Critérios de aceite

**CA1. Cálculo da TaxaAgregada**
DADO QUE o servidor recebeu B BytesAgregados numa JanelaDeMedição de T ns (T > 0)
QUANDO a taxa é calculada
ENTÃO o resultado é B × 8000 / T Mbps.
DADO QUE T = 0, ENTÃO retorna erro `BENCH_ERR_ZERO_TIME`.

**CA2. BytesAgregados e JanelaDeMedição**
DADO QUE há N contadores por cliente e N instantes de EOF
QUANDO agregados
ENTÃO BytesAgregados = soma dos contadores e JanelaDeMedição = max(EOF) − início.
DADO QUE algum EOF é anterior ao início, ou N = 0, ENTÃO retorna erro.

**CA3. Relatório**
DADO QUE a medição terminou
QUANDO o relatório é formatado
ENTÃO é uma linha CSV `clientes,bytes,segundos,mbps` (ex.: `3,1250000000,10.000,1000.00`).

**CA4. Argumentos do servidor**
DADO QUE o servidor recebe `<porta>`, ENTÃO roda em ModoChat.
DADO QUE recebe `<porta> --bench <N> [rcvbuf]`, ENTÃO roda em ModoBenchmark com N ∈ [1,100] e rcvbuf ∈ [0, 64 MiB] (padrão 0).
Valores fora do limite ou não numéricos retornam erro.

**CA5. Argumentos do cliente**
DADO QUE o cliente recebe `<IP> <porta>`, ENTÃO roda em ModoChat.
DADO QUE recebe `<IP> <porta> --bench <seg> [bloco] [nodelay] [sndbuf]`, ENTÃO roda em ModoBenchmark com
seg ∈ [1,3600], bloco ∈ [1, 16 MiB] (padrão 128 KiB), nodelay ∈ {0,1} (padrão 0), sndbuf ∈ [0, 64 MiB] (padrão 0).

**CA6. Execução do experimento (validado pelo `run_bench.sh`)**
DADO QUE o servidor aguarda N ClientesEsperados
QUANDO os N conectam
ENTÃO o servidor envia o SinalDeInício a todos, os clientes transmitem por DuraçãoDeEnvio segundos sem digitação,
e o servidor imprime a linha CSV do CA3 e encerra.

## Design

### Functional Core (`bench_core.c`, puro, testado sem mocks)
```c
LongResult         bench_parse_long(const char *text, long min, long max);
ServerConfigResult bench_parse_server_args(int argc, char *const argv[]);
ClientConfigResult bench_parse_client_args(int argc, char *const argv[]);
uint64_t           bench_total_bytes(const uint64_t *per_client, size_t n);
NsResult           bench_window_ns(uint64_t start_ns, const uint64_t *end_ns, size_t n);
MbpsResult         bench_mbps(uint64_t bytes, uint64_t elapsed_ns);
BenchError         bench_format_report(char *out, size_t cap, int clients,
                                       uint64_t bytes, uint64_t ns, double mbps);
```

### Imperative Shell
- `bench_server.c`: accept dos N clientes, leitura do nome, relógio (`clock_gettime`), envio do SinalDeInício, threads de `recv`, impressão.
- `bench_client.c`: connect, opções de socket, envio do nome, espera do SinalDeInício, laço de `send` até o prazo.
- `server_chat.c` / `client_chat.c`: `main` faz o parse via core e despacha para ModoChat ou ModoBenchmark.