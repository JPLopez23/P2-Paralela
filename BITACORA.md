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
