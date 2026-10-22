#!/bin/bash
# Lanza el ataque de fuerza bruta distribuido desde el nodo MAESTRO,
# limitando OpenMPI a la subred fisica del router (evita que intente
# enrutar por las interfaces virtuales de Docker y se cuelgue).
#
# Uso:
#   scripts/docker/run_cluster.sh <subred_cidr> <hostfile> <np> [argumentos...]
#
# Ejemplo (autoprueba):
#   scripts/docker/run_cluster.sh 192.168.1.0/24 cluster_hosts 8 \
#       --text "frase objetivo dentro del texto" --phrase "frase objetivo" \
#       --key 23 --upper 1000000
#
# Ejemplo (ataque real contra el texto de otro equipo):
#   scripts/docker/run_cluster.sh 192.168.1.0/24 cluster_hosts 8 \
#       --cipher-file datos/texto_otro_equipo.bin --phrase "palabra clave" \
#       --upper 72057594037927936
set -euo pipefail

if [ $# -lt 3 ]; then
    echo "Uso: $0 <subred_cidr> <hostfile> <np> [argumentos del programa...]" >&2
    exit 1
fi

SUBNET="$1"
HOSTFILE="$2"
NP="$3"
shift 3

mpirun --hostfile "$HOSTFILE" \
    --mca btl_tcp_if_include "$SUBNET" \
    --mca oob_tcp_if_include "$SUBNET" \
    -np "$NP" \
    ./build/bruteforce_mpi "$@"
