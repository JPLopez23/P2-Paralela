# Fuerza bruta DES con OpenMPI

## Estructura

- `src/bruteforce_mpi.c`: implementación corregida y paralela. Admite dos
  modos: autoprueba (`--text` + `--key`, cifra y luego busca) y ataque real
  (`--cipher-file`, descifra un binario cuya llave no se conoce).
- `tests/test_core.c`: pruebas del núcleo.
- `datos/texto_350_palabras.txt`: texto original de exactamente 350 palabras.
- `scripts/benchmark.sh`: ejecuta 45 mediciones por defecto.
- `scripts/resumir_resultados.py`: calcula promedio, desviación, speedup y eficiencia.
- `resultados/`: observaciones crudas y resumen calculado.
- `imagenes/`: diagramas SVG del informe.
- `informe.md`: documento de la Parte 2 (clúster con Docker y OpenMPI en red física).
- `Dockerfile`, `scripts/docker/`, `cluster_hosts.example`: entorno e
  instrucciones para correr el ataque distribuido entre varias máquinas
  (ver [Parte 2](#parte-2-clúster-con-docker-y-openmpi) más abajo).

## Dependencias en Ubuntu 24.04

```bash
sudo apt update
sudo apt install build-essential openmpi-bin libopenmpi-dev libssl-dev python3
```

El archivo original usa `rpc/des_crypt.h`. Esa interfaz ya no enlaza en una instalación moderna de Ubuntu; la versión entregada conserva DES/ECB, pero usa la implementación DES de OpenSSL 3 (`libcrypto`).

## Compilación y pruebas

```bash
make
make test
```

Ejecución pequeña:

```bash
mpirun -np 4 ./build/bruteforce_mpi \
  --text "Texto con la frase objetivo" \
  --phrase "frase objetivo" \
  --key 23 \
  --upper 64
```

Condiciones: `0 <= key < upper <= 2^56`; `--phrase` no puede estar vacío ni superar el texto. `upper` es exclusivo.

Para descifrar un archivo cifrado por otro equipo (sin conocer su llave), se usa
`--cipher-file` en vez de `--text` + `--key`, con `--lower`/`--upper` opcionalmente
acotando el espacio de búsqueda:

```bash
mpirun -np 4 ./build/bruteforce_mpi \
  --cipher-file datos/texto_otro_equipo.bin \
  --phrase "palabra clave objetivo" \
  --upper 72057594037927936
```

## Repetir el experimento

```bash
bash scripts/benchmark.sh
python3 scripts/resumir_resultados.py
```

Valores predeterminados:

- Llaves: `4095`, `16383` y `28671`.
- Procesos: `1`, `2` y `4`.
- Cinco repeticiones por combinación.
- Espacio experimental: `[0, 32768)`.
- Frase: `la memoria compartida fortalece a la comunidad`.

Se puede cambiar cualquier parámetro mediante variables de entorno, por ejemplo:

```bash
RUNS=7 PROCESSES="1 2 4 8" KEYS="4095 16383 28671" bash scripts/benchmark.sh
```

En Docker Desktop desde PowerShell:

```powershell
docker run --rm -v "${PWD}:/work" -w /work ubuntu:24.04 bash -lc "apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq build-essential openmpi-bin libopenmpi-dev libssl-dev python3 >/dev/null && make clean && make && make test && bash scripts/benchmark.sh && python3 scripts/resumir_resultados.py"
```

Los tiempos dependen del hardware, carga del sistema, implementación MPI y ubicación de cada llave. Para una comparación válida se deben volver a generar en la máquina o clúster donde se presentará el análisis.

## Parte 2: clúster con Docker y OpenMPI

Detalle completo, tabla de validación y discusión en [informe.md](informe.md).
Resumen de comandos:

```bash
# 1. Construir la imagen (una vez por integrante).
make docker-build

# 2. Abrir una terminal en un contenedor, con la red del host compartida
#    (requisito para que los demás nodos del router lo vean).
make docker-shell

# Dentro del contenedor, en cada nodo (maestro y workers):
bash scripts/docker/start_node.sh

# Desde el maestro, confiar en cada worker (una vez por worker):
bash scripts/docker/trust_worker.sh root@<ip_del_worker>

# Copiar cluster_hosts.example a cluster_hosts y poner las IPs reales,
# luego lanzar el ataque distribuido desde el maestro:
bash scripts/docker/run_cluster.sh 192.168.1.0/24 cluster_hosts <TOTAL_SLOTS> \
  --cipher-file datos/texto_otro_equipo.bin \
  --phrase "palabra clave objetivo" \
  --upper 72057594037927936
```

Notas importantes (ver `informe.md` para la explicación completa):

- Se necesita Docker Desktop 4.34+ con "Enable Host Networking" activado
  (Windows/macOS), y permitir el tráfico entrante en el firewall.
- `--mca btl_tcp_if_include`/`oob_tcp_if_include` deben apuntar a la subred
  real del router, o `mpirun` se cuelga indefinidamente intentando usar la
  interfaz virtual `docker0`.
- `slots`/`max_slots` en el hostfile deben respetar los núcleos físicos de
  cada máquina; sobre-suscribir agresivamente empeora el rendimiento.
