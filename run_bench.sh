#!/usr/bin/env bash
# Executa o experimento de TaxaAgregada (clientes -> servidor) variando de 1 a N clientes.
#
# Uso: ./run_bench.sh [opções]
#   -d SEG    DuraçãoDeEnvio de cada rodada            (padrão 10)
#   -n N      quantidade máxima de clientes            (padrão 10)
#   -r R      repetições por quantidade de clientes    (padrão 3)
#   -b BYTES  BlocoDeEnvio                             (padrão 131072)
#   -t 0|1    TCP_NODELAY nos clientes                 (padrão 0)
#   -s BYTES  SO_SNDBUF dos clientes (0 = autotuning)  (padrão 0)
#   -R BYTES  SO_RCVBUF do servidor  (0 = autotuning)  (padrão 0)
#   -p PORTA  porta TCP                                (padrão 5000)
#   -o ARQ    CSV de saída                             (padrão resultados.csv)
#
# Saída: CSV "clientes,rodada,bytes,segundos,mbps" + resumo (média/mín/máx) no terminal.
set -uo pipefail

DUR=10; MAXN=10; REPS=3; BLOCK=131072; NODELAY=0; SNDBUF=0; RCVBUF=0
PORT=5000; OUT=resultados.csv

while getopts "d:n:r:b:t:s:R:p:o:h" opt; do
    case "$opt" in
        d) DUR=$OPTARG ;;   n) MAXN=$OPTARG ;;  r) REPS=$OPTARG ;;
        b) BLOCK=$OPTARG ;; t) NODELAY=$OPTARG ;; s) SNDBUF=$OPTARG ;;
        R) RCVBUF=$OPTARG ;; p) PORT=$OPTARG ;; o) OUT=$OPTARG ;;
        *) sed -n '2,15p' "$0"; exit 1 ;;
    esac
done

cd "$(dirname "$0")"
make -s server client || exit 1

LOGDIR=$(mktemp -d)
trap 'rm -rf "$LOGDIR"' EXIT

# Espera o servidor anunciar que está escutando (sem abrir conexão de teste,
# que seria contada como cliente).
wait_listening() {
    for _ in $(seq 1 100); do
        grep -q "aguardando" "$1" 2>/dev/null && return 0
        sleep 0.05
    done
    return 1
}

# Uma rodada: sobe o servidor esperando N clientes, dispara N clientes em paralelo
# e imprime a linha CSV produzida pelo servidor.
run_round() {
    local n=$1 slog="$LOGDIR/server.err" sout="$LOGDIR/server.out"
    : > "$slog"; : > "$sout"
    ./server "$PORT" --bench "$n" "$RCVBUF" > "$sout" 2> "$slog" &
    local spid=$!
    if ! wait_listening "$slog"; then
        echo "servidor não subiu:" >&2; cat "$slog" >&2; kill "$spid" 2>/dev/null; return 1
    fi
    for _ in $(seq 1 "$n"); do
        ./client 127.0.0.1 "$PORT" --bench "$DUR" "$BLOCK" "$NODELAY" "$SNDBUF" \
            2>> "$LOGDIR/clients.err" &
    done
    wait "$spid" || { cat "$slog" >&2; return 1; }
    wait
    cat "$sout"
}

echo "Config: dur=${DUR}s bloco=${BLOCK}B nodelay=${NODELAY} sndbuf=${SNDBUF} rcvbuf=${RCVBUF} reps=${REPS}"
echo "clientes,rodada,bytes,segundos,mbps" > "$OUT"

for n in $(seq 1 "$MAXN"); do
    for r in $(seq 1 "$REPS"); do
        line=$(run_round "$n") || exit 1
        echo "$line" | awk -F, -v r="$r" '{ print $1 "," r "," $2 "," $3 "," $4 }' >> "$OUT"
        echo "  n=$n rodada=$r -> $(echo "$line" | cut -d, -f4) Mbps"
    done
done

echo
echo "Resumo (TaxaAgregada em Mbps):"
awk -F, 'NR > 1 {
    n = $1; v = $5; s[n] += v; c[n]++
    if (!(n in mn) || v < mn[n]) mn[n] = v
    if (!(n in mx) || v > mx[n]) mx[n] = v
} END {
    printf "%-9s %12s %12s %12s\n", "clientes", "media", "min", "max"
    for (n = 1; n in c; n++)
        printf "%-9d %12.1f %12.1f %12.1f\n", n, s[n] / c[n], mn[n], mx[n]
}' "$OUT"
echo
echo "CSV salvo em $OUT"