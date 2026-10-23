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
#
# Nodos Windows (Docker Desktop): si algun nodo del hostfile corre en
# Windows, su contenedor NO pudo usar --network host (Docker Desktop no
# comparte ahi la red real; ver BITACORA.md) y en su lugar publica un
# rango fijo de puertos con -p 50000-50019:50000-50019. En ese caso hay
# que exportar FIXED_PORTS=1 para que OpenMPI tambien use ese rango fijo
# en vez de puertos aleatorios (que no estarian publicados ni permitidos
# por el firewall):
#
#   FIXED_PORTS=1 scripts/docker/run_cluster.sh 172.20.10.0/24 cluster_hosts 3 \
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

PORT_ARGS=()
if [ "${FIXED_PORTS:-0}" = "1" ]; then
    PORT_ARGS=(
        --mca btl_tcp_port_min_v4 50000 --mca btl_tcp_port_range_v4 10
        --mca oob_tcp_port_min_v4 50010 --mca oob_tcp_port_range_v4 10
    )
fi

mpirun --hostfile "$HOSTFILE" \
    --mca btl_tcp_if_include "$SUBNET" \
    --mca oob_tcp_if_include "$SUBNET" \
    "${PORT_ARGS[@]}" \
    -np "$NP" \
    ./build/bruteforce_mpi "$@"
