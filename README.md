# Chat TCP: Maximizado

Aplicação de chat TCP (servidor multithread + cliente) com um **Modo Benchmark** adicional para medir a capacidade máxima de transmissão dos clientes para o servidor, de 1 a 10 clientes simultâneos.

Os critérios de aceite e o que ainda falta no projeto estão em [`SPEC.md`](SPEC.md).

## Compilar e rodar

```sh
make            # gera ./server e ./client
```

**ModoChat (comportamento original, sem mudanças):**
```sh
./server 5000
./client 127.0.0.1 5000
```

**ModoBenchmark:**
```sh
./server <porta> --bench <n_clientes> [rcvbuf]
./client <IP> <porta> --bench <segundos> [bloco] [nodelay 0|1] [sndbuf]
```
| Parâmetro | Padrão | Significado |
|---|---|---|
| `n_clientes` | obrigatório (1–100) | ClientesEsperados: o servidor só inicia a medição quando todos conectarem |
| `segundos` | obrigatório (1–3600) | DuraçãoDeEnvio de cada cliente |
| `bloco` | 131072 (128 KiB) | BlocoDeEnvio: bytes por chamada `send()` |
| `nodelay` | 0 | 1 liga `TCP_NODELAY` |
| `sndbuf` / `rcvbuf` | 0 | `SO_SNDBUF` / `SO_RCVBUF`; 0 mantém o autotuning do kernel |

O servidor imprime uma linha CSV no stdout: `clientes,bytes,segundos,mbps`. Cada cliente imprime no stderr os bytes que enviou e a taxa vista por ele (só informativa).

**Experimento completo que recomendo fazer para a análise (1 a 10 clientes):**

```sh
./run_bench.sh                 # 10 s por rodada, 3 rodadas, resultados.csv

#ou

./run_bench.sh -d 5 -r 5 -t 1  # exemplo: 5 s, 5 rodadas, TCP_NODELAY ligado

#ou 

./run_bench.sh -h              # mostra as opções e auxilia a customizar da forma desejada.
```

**Em rede real (duas máquinas):** o `run_bench.sh` roda cliente e servidor na mesma máquina (127.0.0.1). Para medir uma rede de verdade, rode uma rodada por quantidade de clientes:
```sh
# máquina servidora
./server 5000 --bench 4
# máquina cliente
for i in $(seq 4); do ./client <IP_DO_SERVIDOR> 5000 --bench 10 & done; wait
```

---

## Alterações e motivos

### 1. Modo Benchmark separado e sem broadcast
- **O que mudou exatamente?** `server_chat.c` e `client_chat.c` agora só fazem o parse dos argumentos e chamam o ModoChat (código original, movido para `run_chat_server` / `run_chat_client`) ou o ModoBenchmark (`bench_server.c` / `bench_client.c`).
- **Por quê?** no ModoChat, cada linha recebida é reenviada a todos os outros clientes (`broadcast`) com um mutex global travado. Com N clientes, o servidor envia cerca de N−1 vezes o que recebe, e todas as threads disputam o mesmo lock. Dessa forma, ao invés de refletir a vazão cliente/servidor a medição passaria a medir o broadcast. No ModoBenchmark o servidor só recebe e descarta.
- **Por que não alterar o ModoChat:** o chat continua funcionando exatamente como antes. O benchmark é um caminho à parte, ativado por `--bench`. Para ser possível comparar.

### 2. Leitura e escrita em blocos grandes
- **O que mudou?** o servidor em ModoBenchmark faz `recv()` com um buffer de 256 KiB por conexão. O cliente envia BlocosDeEnvio de 128 KiB em laço, sem enquadramento por linha.
- **Por quê?** o código original lê com `recv(fd, &c, 1, 0)`, ou seja, **uma chamada de sistema por byte**, o que torna mais lento.
- **Envios parciais:** como o conteúdo é irrelevante e não há delimitador, um `send()` parcial só soma os bytes enviados e o laço continua. Não é preciso reenviar o resto do bloco.

### 3. `run_bench.sh`
- Sobe o servidor, espera ele anunciar que está escutando e dispara N clientes em paralelo. Faz isso de 1 a 10 clientes, com R repetições cada.
- Gera um CSV (`clientes,rodada,bytes,segundos,mbps`) e mostra um resumo com média, mínimo e máximo.
- A espera pelo servidor lê o log em vez de testar a porta, porque uma conexão de teste seria contada como cliente.

### 4. Arquitetura (Functional Core / Imperative Shell)
- **Functional Core**, em `bench_core.c`/`.h`: só funções puras, sem I/O, relógio ou alocação, com erros retornados em tipos Result (`LongResult`, `MbpsResult`, `NsResult`, `ServerConfigResult`, `ClientConfigResult`):
  - `bench_parse_long`, `bench_parse_server_args`, `bench_parse_client_args`: validação de argumentos e limites.
  - `bench_total_bytes`, `bench_window_ns`, `bench_mbps`: agregação e cálculo da TaxaAgregada.
  - `bench_format_report`: linha CSV.
- **Imperative Shell**, em `bench_server.c` e `bench_client.c`: sockets, threads, relógio e impressão. Segue o fluxo *gather* (aceitar conexões, ler o relógio, receber bytes) → *decide* (o núcleo calcula) → *act* (imprimir). Por ser I/O, essa parte é validada pelo próprio experimento, conforme o CA6 do `SPEC.md`.

---

## Resultados (Em breve)


## Arquivos

| Arquivo | Papel |
|---|---|
| `server_chat.c` | `main` do servidor: parse e escolha entre ModoChat e ModoBenchmark; ModoChat original |
| `client_chat.c` | `main` do cliente: parse e escolha entre ModoChat e ModoBenchmark; ModoChat original |
| `bench_core.c/.h` | Functional Core (puro, testado) |
| `bench_server.c/.h` | Imperative Shell do servidor em ModoBenchmark |
| `bench_client.c/.h` | Imperative Shell do ClienteBenchmark |
| `tests/test_bench_core.c` | Testes unitários do núcleo |
| `run_bench.sh` | Experimento de 1 a N clientes |
| `SPEC.md` | Escopo, critérios de aceite, design e tarefas |
