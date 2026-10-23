# Bitácora: búsqueda del reto del Grupo 7

## Grupo 9 — 2026-10-21 — llave verificada

- Entrada: base64 proporcionado por el usuario, conservado en
  `datos/grupo9_cifrado.b64` y decodificado en `datos/grupo9_cifrado.des`.
  Longitud: 2168 bytes. SHA-256 local y remoto:
  `0f9eedc5f395e714b83e7571b3d50d13290a99f8ca9d3f205ee5fa557dbf4f18`.
- Frase exacta: `computación paralela y distribuida`.
- Rango autorizado: desde 0 hasta 2^24 inclusive (`[0,16777217)`).
- **Llave lógica: 3141592. Llave DES con paridad: b0bf7f0201010101.**
  Modo DES-ECB y conversión `DES_KEY_LAYOUT=1` (grupos de siete bits LE).
- El worker `172.20.10.14`, rank 0 de cinco procesos MPI, encontró la
  candidata. Tiempo reportado por el diagnóstico layout 1: 3.610660 s;
  no representa la duración total de todos los intentos ni un benchmark
  del buscador original. Layout 0 no produjo candidatas heurísticas.
- Se verificó de forma independiente con la CLI OpenSSL local: texto
  coherente completo que contiene la frase exacta. Los últimos seis bytes
  son espacios, no relleno PKCS#7; se conservaron intactos con `-nopad`.
- Al recifrar con OpenSSL, `cmp` confirmó igualdad byte a byte con el
  ciphertext recibido y el SHA-256 anterior coincidió.
- El buscador MPI principal, compilado con `DES_KEY_LAYOUT=1`, confirmó
  la frase exacta en `[3141592,3141593)`. Esta comprobación de una sola llave
  no es una nueva búsqueda ni una medición de fuerza bruta.
- La tanda diagnóstica terminó: conversiones ECB 0 a 14 completadas,
  únicamente layout 1 produjo una candidata. No quedan trabajos de esta
  tanda activos. `make test`: ocho pruebas superadas; `git diff --check`: limpio.
- Archivos recuperados: `resultados/grupo9_descifrado.txt` y
  `resultados/grupo9_recifrado.des`. No se modificaron los resultados del
  Grupo 7. El diagnóstico ahora usa nombres temporales genéricos
  `/tmp/des-candidata-layoutN-keyK.txt` para no etiquetar otros retos como Grupo 7.

Comando reproducible (OpenSSL 3):

```sh
openssl enc -des-ecb -d -nopad -provider legacy -provider default \
  -K b0bf7f0201010101 -in datos/grupo9_cifrado.des \
  -out resultados/grupo9_descifrado.txt
```

## Resultado confirmado — rango corregido a 2^24

- El usuario corrigió el límite: **2^24**, no 2^22. Se buscó desde cero
  con límite exclusivo 16777217 para incluir también el extremo.
- **Llave lógica encontrada: 12989899.** DES-ECB, conversión `DES_KEY_LAYOUT=1`
  (grupos de siete bits en orden little-endian), llave hexadecimal con
  paridad ajustada: **97d6310d01010101**.
- Encontrada por el rank 3 del worker `172.20.10.14`, con cinco procesos.
  El diagnóstico de esa conversión reportó 1.410655 s; no es el tiempo total
  de todos los intentos ni una medición comparable al buscador original.
- Se utilizó como referencia el texto suministrado por el usuario, guardado
  en `datos/grupo7_texto_candidato.txt` (2145 bytes UTF-8 con salto final).
  La comparación exacta de 24 bytes reemplazó el filtro heurístico.
- Se corrigió un error inicial del diagnóstico: el lector del ciphertext
  rechazaba texto conocido de longitud no múltiplo de ocho. Esas ejecuciones
  abortadas no cuentan como búsquedas completadas.
- Validación independiente mediante la CLI OpenSSL local (la CLI no estaba
  instalada en el worker): descifrado PKCS#7 correcto, siete bytes de relleno;
  `cmp` confirmó igualdad exacta con todo el texto suministrado. Al recifrar,
  `cmp` confirmó igualdad byte a byte con el archivo original y SHA-256
  `285de3acef0f7d8371666b74f66edc3026b5721566f8c8a69fcc9acb7b41e596`.
- Artefactos: `resultados/grupo7_descifrado.txt` y
  `resultados/grupo7_recifrado.des`. **Reto criptográfico resuelto.**
- La llave encontrada supera 2^22: el límite anterior la excluía.

Verificación reproducible con OpenSSL 3:

```sh
openssl enc -des-ecb -d -provider legacy -provider default \
  -K 97d6310d01010101 -in /Users/joseruiz/Downloads/cifrado_foro-1.des \
  -out resultados/grupo7_descifrado.txt
cmp resultados/grupo7_descifrado.txt datos/grupo7_texto_candidato.txt
```

## 2026-10-21

- Objetivo: recuperar la llave del archivo `cifrado_foro-1.des`, buscando
  `tolerancia a fallos`. El usuario confirmó que el rango empieza en **0**.
- Archivo de 2152 bytes. SHA-256 verificado en original, maestro y worker:
  `285de3acef0f7d8371666b74f66edc3026b5721566f8c8a69fcc9acb7b41e596`.
- Se recorrió el rango `[0, 4194305)`: incluye también el extremo `2^22`
  para evitar una discrepancia de límite inclusivo/exclusivo.
- Se realizaron búsquedas completas de la frase con varias conversiones de
  llave DES, sin coincidencia. Se permitió buscar la frase aunque el relleno
  no fuera PKCS#7, para admitir archivos con relleno cero.
- El worker `172.20.10.14` ejecutó cálculos reales con cinco procesos MPI,
  usando `--use-hwthread-cpus`. Las ejecuciones remotas se lanzaron por SSH.
  Esto no equivale a un único trabajo MPI entre ambas máquinas: la conectividad
  MPI entre las redes virtuales de los contenedores sigue sin resolverse.
- Se añadió `scripts/diagnosticar_des.c`, diagnóstico experimental que prueba
  texto plausible sin exigir la frase exacta, incluyendo ceros compatibles
  con UTF-16. Examina muestras iniciales/intermedias antes del texto completo.
  Es un filtro heurístico: puede descartar texto válido, comprimido o binario;
  un resultado negativo no demuestra que la llave esté fuera del rango.
- Se incorporaron variantes de conversión seleccionables mediante
  `DES_KEY_LAYOUT`, además de CBC con IV cero y paridad heredada opcionales.
  El comportamiento predeterminado sigue siendo DES-ECB, conversión 0.
- Se validó el diagnóstico con un cifrado sintético de llave 37 y texto
  conocido: recuperó correctamente la llave y el texto. `make test` pasó
  las ocho pruebas del núcleo; estas usan una transformación simulada, no
  sustituyen la validación criptográfica anterior. `git diff --check` pasó.
- Una tanda remota se interrumpió al cambiar el contenedor del worker
  (`1c46721f24ee` a `c52b4de8a644`). Se comprobó SSH, se repusieron fuentes y
  ciphertext y se reinició el diagnóstico con cinco procesos desde cero.
- La tanda reiniciada terminó con código de salida 0: las 15 conversiones
  (0 a 14), tanto en ECB como en CBC con IV cero, recorrieron el rango
  completo y reportaron cero candidatas según el filtro heurístico.
  **Estado: llave todavía no recuperada; no quedan búsquedas de esta tanda
  ejecutándose.**

## Información necesaria para confirmar interoperabilidad

Solicitar al Grupo 7 su código de cifrado o la descripción exacta del modo,
conversión del entero a los ocho bytes DES, IV (si aplica), relleno y formato
del archivo. No se necesita que revelen la llave. Hasta recuperar y validar
el texto no debe registrarse el reto como resuelto.

## Despliegue físico del clúster — diagnóstico de red (21-oct)

Equipo conectado al hotspot de un iPhone (red `172.20.10.0/28`): Mac del compañero como **maestro** (`172.20.10.10`, usando OrbStack), esta PC Windows como **worker** (`172.20.10.14`, Docker Desktop).

### Problema 1: `--network host` en Docker Desktop para Windows no expone la red real

Al correr `scripts/docker/start_node.sh` dentro de un contenedor con `--network host`, las IPs que mostraba el contenedor eran `192.168.65.x` y rangos `172.17-21.x` (redes internas de Docker), **nunca** la IP real de Windows en el hotspot (`172.20.10.14`). Diagnóstico paso a paso:

1. Se confirmó con `ipconfig` que la IP real del adaptador Wi-Fi de Windows sí era `172.20.10.14`.
2. `Test-NetConnection` **desde la misma Windows** hacia `172.20.10.14:22` funcionaba — pero esto fue engañoso: Windows redirige automáticamente conexiones locales dirigidas a su propia IP externa hacia dentro de WSL2 (un comportamiento conocido de "localhost forwarding"), así que ese resultado no demuestra alcance real desde otra máquina.
3. El `ping` entre ambas máquinas sí funcionaba en ambos sentidos (descartando aislamiento de clientes del hotspot).
4. Se revisó `.wslconfig` (`C:\Users\jplop\.wslconfig`) y **no tenía** `networkingMode=mirrored` — se agregó, y se confirmó con `wsl -d docker-desktop -- ip addr` que la VM de Docker Desktop sí pasó a tener la IP real `172.20.10.14` directamente.
5. Aun así, **el contenedor** (incluso con `--network host`) seguía sin ver esa interfaz — solo veía las redes internas de Docker (`192.168.65.3/24`, puentes `172.17-21.0.1`). Conclusión: en Docker Desktop para Windows, `--network host` comparte la red interna de la VM de Docker, **no** la interfaz real de Windows, sin importar si WSL está en modo mirrored. Es una limitación propia de Windows, distinta a Linux/Mac (donde sí funciona como describe el PDF).

**Solución aplicada:** dejar de usar `--network host` en Windows y en su lugar publicar el puerto explícitamente:
```powershell
docker run -dit -p 22:22 --name mpi_node mpi-cluster-image /bin/bash
```
Docker Desktop sí reenvía correctamente el tráfico que llega a la IP real de Windows en el puerto publicado, independientemente del problema de `--network host`.

### Problema 2: firewall de Windows bloqueando el puerto 22 entrante

El adaptador Wi-Fi estaba en perfil **Público** (el más restrictivo), sin ninguna regla que permitiera el puerto 22 de entrada. Se agregó una regla mínima (solo TCP 22, solo desde la subred del hotspot, no se desactivó el firewall completo):
```powershell
New-NetFirewallRule -DisplayName "MPI-Cluster SSH 22 (LAN)" -Direction Inbound -Protocol TCP -LocalPort 22 -RemoteAddress 172.20.10.0/24 -Profile Any -Action Allow
```

### Problema 3: intercambio de llaves SSH

Con la red y el firewall resueltos, el maestro ya lograba conectar por TCP pero rechazaba la autenticación (esperado: todavía no se había copiado la llave pública del maestro al worker). Se agregó manualmente la llave pública del maestro (`root@orbstack`) a `/root/.ssh/authorized_keys` del contenedor worker, con permisos `700`/`600`.

### Problema 4: puertos dinámicos de MPI (BTL/OOB) en el worker de Windows

`--network host` no estaba disponible para el worker Windows (ver Problema 1), así que OpenMPI tampoco puede usar puertos aleatorios ahí: Docker solo reenvía los puertos que se publicaron explícitamente al crear el contenedor.

**Solución:** se fijó un rango de 20 puertos TCP para este worker y se publicaron todos:
```powershell
docker run -dit -p 22:22 -p 50000-50019:50000-50019 --name mpi_node mpi-cluster-image /bin/bash
```
Y una regla de firewall equivalente para ese rango.

`scripts/docker/run_cluster.sh` ahora acepta `FIXED_PORTS=1` para que el `mpirun` del maestro use ese mismo rango fijo (`--mca btl_tcp_port_min_v4 50000 --mca btl_tcp_port_range_v4 10 --mca oob_tcp_port_min_v4 50010 --mca oob_tcp_port_range_v4 10`) en vez de puertos aleatorios no publicados:
```bash
FIXED_PORTS=1 bash scripts/docker/run_cluster.sh 172.20.10.0/24 cluster_hosts <TOTAL_SLOTS> \
  --cipher-file datos/cifrado_grupo7.des --phrase "tolerancia a fallos" --upper 16777216
```

Nota: si hay más de un worker Windows en el clúster, cada uno necesita su propio rango de puertos publicado sin solaparse (ej. el segundo worker Windows usaría `50020-50039`), porque Docker no puede publicar el mismo puerto dos veces en la misma máquina.

### Estado al 21-oct: maestro y primer worker conectados por SSH sin contraseña

Falta integrar al tercer integrante del equipo y correr el Paso B (ataque distribuido real) con los tres nodos.

**Nota (22-oct):** el equipo ya no está todos en la misma red física. Se decidió usar una VPN mesh (Tailscale) para reconectar virtualmente a los 3 sin depender de estar en el mismo router/hotspot — mismo procedimiento de arriba, solo cambian las IPs (de `172.20.10.x` a las `100.x.x.x` que asigna Tailscale) y las reglas de firewall/`--mca ..._if_include` deben apuntar a `100.64.0.0/10` en vez de `172.20.10.0/24`.

### ✅ 23-oct — Paso B logrado: ataque distribuido real con los 3 nodos

Problema adicional con Tailscale: los contenedores sin `--network host`
(los Windows) no tenían ninguna interfaz propia dentro de `100.64.0.0/10`,
así que `--mca btl_tcp_if_include`/`oob_tcp_if_include` no encontraban
nada que usar. Solución: dentro de cada contenedor Windows,

```bash
docker commit mpi_node mpi-cluster-worker-ready
docker run -dit --cap-add NET_ADMIN --sysctl net.ipv4.ip_nonlocal_bind=1 \
  -p 22:22 -p 50000-50019:50000-50019 --name mpi_node mpi-cluster-worker-ready bash
docker exec mpi_node bash -lc "ip link add tsmpi type dummy && \
  ip addr add <ip_tailscale>/10 dev tsmpi noprefixroute && ip link set tsmpi up && \
  bash scripts/docker/start_node.sh"
```

Esto crea una interfaz de red virtual (`tsmpi`, tipo `dummy`) dentro del
contenedor con la IP de Tailscale de esa máquina asignada, más
`net.ipv4.ip_nonlocal_bind=1` para permitir enlazarse a esa dirección
aunque el kernel no la reconozca como "local" por defecto. Con eso,
OpenMPI ya encontraba una interfaz válida dentro del rango de Tailscale.

**Cambio de rol:** el nodo maestro originalmente planeado (este worker,
`JPLopez23`) no logró completar su propia conexión a Tailscale a tiempo
(problema de cuenta: quedó en su tailnet personal por accidente en vez de
la del equipo — se resolvió con `tailscale logout` y volver a entrar
desde el correo de invitación, pero ya tarde). **Fer tomó el rol de
maestro.** Este equipo terminó como worker.

**Comando final, ejecutado por Fer (maestro) el 23-oct:**

```
Get-Content cluster_hosts
100.93.43.1 slots=1 max_slots=1
100.110.1.47 slots=1 max_slots=1
100.119.140.37 slots=1 max_slots=1

mpirun --allow-run-as-root --hostfile cluster_hosts --map-by ppr:1:node \
  --mca oob_tcp_if_include tsmpi --mca oob_tcp_static_ipv4_ports 50009 \
  --mca btl self,tcp --mca btl_tcp_if_include tsmpi \
  --mca btl_tcp_port_min_v4 50010 --mca btl_tcp_port_range_v4 1 \
  -np 3 ./build/bruteforce_mpi \
  --cipher-file datos/cifrado_grupo7.des --phrase 'tolerancia a fallos' --upper 16777216

Nodo 0d9ce0b91b4e, proceso 0 de 3: rango [0, 5592406)
Nodo 39e08d63be0e, proceso 1 de 3: rango [5592406, 11184811)
Nodo 72035ee6bfda, proceso 2 de 3: rango [11184811, 16777216)
Nodo 72035ee6bfda, proceso 2: encontro una candidata en 12989899
Llave encontrada: 12989899
Tiempo: 66.56248 s
Texto: Un clúster de computadoras es un conjunto de máquinas independientes...
Exit code: 0
```

**3 nodos distintos participaron de verdad** (`0d9ce0b91b4e` = maestro,
`39e08d63be0e` = Oli, `72035ee6bfda` = este worker, que fue el que
encontró la candidata). Comparado con el Ts local (329.74s, 1 proceso),
el clúster real con 3 procesos en 3 máquinas distintas tardó 66.56s
(~4.95x, superlineal otra vez por la posición de la llave en su
segmento — mismo fenómeno ya documentado arriba, no una medida de
eficiencia real). **Paso B completado y verificado.**

### Ts medido con el binario actual (compilado con `DES_KEY_LAYOUT=1`)

Sobre el rango completo `[0, 2^24)` contra `datos/cifrado_grupo7.des`:

| Procesos | Tiempo (s) |
| --- | --- |
| 1 | 329.74 |
| 2 | 111.49 |
| 4 | 11.87 |

Los speedups resultantes (2.96 y 27.79) salen superlineales — no es un error, es porque la llave real cae en una posición del rango que, al particionarse distinto según el número de procesos, le toca un segmento más corto a 4 procesos. No es una medida confiable de eficiencia real; para eso habría que promediar varias llaves distintas como en el benchmark de la Fase 1.

---

## Registro anterior del proyecto (conservado del commit remoto)

Los apartados marcados como pendientes describen el estado anterior a las llaves verificadas arriba.

CC3069 - Computación Paralela y Distribuida. Repo: https://github.com/JPLopez23/P2-Paralela

## Parte 1 — Fuerza bruta DES paralela (local)

| Fecha | Autor | Qué se hizo |
| --- | --- | --- |
| 15-oct | JPLopez23 | Código base (`bruteforce_.c`), Makefile y texto de prueba (350 palabras). |
| 16-oct | josfer720 | `compute_range`, relleno PKCS#7 y cifrado/descifrado DES en `src/bruteforce_mpi.c`. |
| 17-oct | 0liRem | Parseo de opciones de línea de comandos, `search_range`/`try_key` y README inicial. |
| 17-oct | JPLopez23 | `main()` con MPI (reparto de rango, `MPI_Allreduce` por lotes) y `scripts/resumir_resultados.py`. |
| 18-oct | josfer720 | `tests/test_core.c` (pruebas del núcleo) y `scripts/benchmark.sh`. |
| 19-oct | 0liRem | `resultados/` (mediciones, resumen) e imágenes del informe. |

Repartido en 6 commits, 2 por persona, subidos en días distintos para simular el trabajo real del equipo. Todo esto ya está en `main` en GitHub.

## Parte 2 — Clúster con Docker y OpenMPI

### 21-oct — Infraestructura Docker (subido: `2a7797d`, JPLopez23)

- `Dockerfile`: imagen `ubuntu:22.04` con `build-essential`, `openmpi-bin`, `libopenmpi-dev`, `libssl-dev`, `openssh-server`.
- `PermitRootLogin yes`, `StrictHostKeyChecking no`, `OMPI_ALLOW_RUN_AS_ROOT(_CONFIRM)=1`.
- Tabla de validación de la sección IV del PDF, ejecutada y confirmada en Docker:
  - `mpicc --version` → gcc 11.4.0 ✔
  - `mpirun --allow-run-as-root -np 2 echo` → "Proceso MPI ejecutado" x2 ✔ (nota: hace falta el flag `--allow-run-as-root` en la línea de comandos, las variables de entorno solas no bastan).
  - `service ssh start` + `ssh-keygen` + `ssh localhost` → conexión libre exitosa ✔

### 21-oct — Modo de ataque real (`--cipher-file`) — pendiente de subir

Se modificó `src/bruteforce_mpi.c` para que el binario soporte dos modos:

- **Autoprueba** (el de antes): `--text` + `--key`, cifra localmente y luego busca.
- **Ataque real**: `--cipher-file RUTA` + `--phrase` + `--upper` (y `--lower` opcional), lee un binario cifrado de otro equipo sin conocer su llave.

Además cada proceso imprime su nodo (`gethostname`) y su rango de búsqueda, para poder evidenciar en el clúster físico qué máquinas participaron (requisito de la sección V/VII del PDF). Se agregaron 7 pruebas en `tests/test_core.c` (antes 6), incluyendo validación de las opciones nuevas y de `read_cipher_file`.

Verificado en Docker con DES real (no el modo de prueba XOR):
- Autoprueba: llave 23 en `[0,64)` → encontrada por el proceso 1 de 4, texto recuperado correcto.
- `--cipher-file`: se generó un cifrado real con llave 777777 y se encontró en `[700000, 800000)` con 4 procesos, texto recuperado correcto.

### 21-oct — Scripts de orquestación y herramientas — pendiente de subir

- `scripts/docker/start_node.sh`: arranca `sshd` en un contenedor y genera/registra su propia llave.
- `scripts/docker/trust_worker.sh`: desde el maestro, copia su llave pública a un worker (`ssh-copy-id`).
- `scripts/docker/run_cluster.sh`: arma el `mpirun --hostfile ... --mca btl_tcp_if_include/oob_tcp_if_include <subred> -np <N>` para no colgarse con la interfaz virtual de Docker.
- `cluster_hosts.example`: hostfile de ejemplo con `slots`/`max_slots`.
- `scripts/decodificar_cifrado.py`: convierte el base64 que publican otros equipos en Canvas al binario que espera `--cipher-file`.
- `scripts/cifrar_para_foro.c`: cifra un texto propio con una llave DES (para generar el reto que publicamos nosotros en el foro) y lo muestra en base64.
- `README.md` e `informe.md` actualizados con instrucciones y la discusión técnica de la Parte 2 (preguntas 1, 4 y 5 ya respondidas; 2, 3 y 6 dependen de la corrida física real).

### 21-oct — Intercambio en el foro de Canvas

**Reto que elegimos atacar:** Grupo 7 "Gatos explosivos".
- Archivo recibido: `cifrado_foro-1.des` (2152 bytes, múltiplo de 8 — formato DES válido), guardado en `datos/cifrado_grupo7.des`.
- Frase objetivo: `tolerancia a fallos`.
- Rango de llave publicado por ellos: `2^22` (0 a 4 194 304, asumiendo que empieza en 0).

**Reto que publicamos nosotros** (para que otro equipo nos ataque):
- Frase objetivo: `escalabilidad horizontal`.
- Texto cifrado (base64) publicado en el foro, generado con `scripts/cifrar_para_foro.c`.
- Llave real usada: `54321` (**no se publicó**; registrada aquí solo para nuestro propio control). Verificado: se recupera el texto completo buscando en `[54000, 54500)`.
- Guardado en `datos/cifrado_para_foro.bin`.

### ⚠️ Pendiente / problema abierto: no se encontró la llave de Grupo 7 en el rango dado

Se corrió la búsqueda completa `[0, 4194304)` contra `datos/cifrado_grupo7.des` buscando `"tolerancia a fallos"` y **no apareció ninguna coincidencia** (79 s con 1 proceso, exit code 3 = "no encontrada").

**Investigación adicional (21-oct, con `scripts/` temporales fuera del repo):** para descartar que fuera solo una diferencia en la fórmula de expansión de la llave lógica a los 8 bytes de DES, se probaron **5 esquemas distintos** sobre el mismo rango `[0, 2^22)`:

- A: el nuestro (7 bits por byte, MSB primero).
- B: la fórmula literal del `bruteforce_.c` original (bucle con máscara `0xFE`, bytes crudos del `long` en little-endian).
- C: llave cruda de 7 bytes, byte completo, MSB primero.
- D: igual que C pero LSB primero.
- E: los 8 bytes crudos del `long`, MSB primero (sin el bucle `0xFE`).

Además se probó con comparación de la frase **sin distinguir mayúsculas/minúsculas** (buscando solo `"tolerancia"`, no la frase completa), para descartar diferencias de capitalización.

**Resultado: ninguno de los 5 esquemas, en ~10 minutos de CPU combinados, encontró la frase en `[0, 2^22)`.** Esto hace poco probable que el problema sea solo la fórmula de expansión de la llave (ya cubrimos las variantes más razonables) o la capitalización. Hipótesis que quedan abiertas, en orden de probabilidad:

1. **El rango no empieza en 0.** "Rango de llave: 2^22" puede significar "el tamaño del espacio de búsqueda es 2^22", sin que la llave real esté entre 0 y 2^22 — podría estar en cualquier ventana de ese tamaño dentro del espacio completo de 56 bits. **Esto es lo más probable y lo primero que hay que confirmar con Grupo 7.**
2. El modo de cifrado no es ECB puro (por ejemplo CBC con un IV al inicio del archivo), lo cual rompería la decodificación por bloques independientes que asumimos.
3. El texto real no contiene literalmente la frase `"tolerancia a fallos"` (con tildes distintas, mayúsculas, puntuación pegada, u otra redacción).

**Qué falta para resolverlo:**
1. **Preguntar directamente a Grupo 7** si el rango `[0, 2^22)` empieza en 0, o si hay un punto de partida (offset) distinto.
2. Si confirman que empieza en 0 y sigue sin aparecer, pedir su código fuente (o al menos su función de cifrado) para replicar exactamente su esquema.
3. Esto es importante discutirlo con el equipo: puede ser un problema que afecte a *todos* los cruces entre equipos de la clase si cada quien "corrigió" el programa original de forma distinta, o si el enunciado del rango de llave en el foro es ambiguo para todos.
