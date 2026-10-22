# Informe - Proyecto 2, Parte 2: HPC con OpenMPI colectivo

CC3069 - Computación Paralela y Distribuida, Semestre II - 2026

> Este informe cubre la Parte 2 de la consigna. La Parte 1 (algoritmo de
> fuerza bruta DES paralelizado localmente) está documentada en
> [README.md](README.md) y en `resultados/`.

## III. Fase de validación: descifrado cruzado local

| Equipo que eligieron | Texto | Palabra objetivo |
| --- | --- | --- |
| _Pendiente: completar con el equipo y archivo elegidos en el foro de Canvas_ | `datos/<archivo_del_otro_equipo>.bin` | `_pendiente_` |

El binario ya soporta este caso sin modificar el texto original de nadie:
en vez de cifrar un texto propio con una llave conocida (modo de autoprueba),
lee directamente el archivo cifrado de otro equipo y busca la llave que
revela la frase objetivo.

```bash
./build/bruteforce_mpi \
  --cipher-file datos/<archivo_del_otro_equipo>.bin \
  --phrase "<palabra clave objetivo>" \
  --upper 72057594037927936
```

Para una corrida más rápida en una sola máquina (local, antes del clúster),
se puede paralelizar igual que en la Fase 1:

```bash
mpirun -np 4 ./build/bruteforce_mpi \
  --cipher-file datos/<archivo_del_otro_equipo>.bin \
  --phrase "<palabra clave objetivo>" \
  --upper 72057594037927936
```

### Log de tiempo y speedup (referencia Ts)

| Procesos | Tiempo (s) | Speedup vs. 1 proceso | Eficiencia |
| --- | --- | --- | --- |
| 1 | _pendiente_ | 1.00 | 100% |
| 2 | _pendiente_ | _pendiente_ | _pendiente_ |
| 4 | _pendiente_ | _pendiente_ | _pendiente_ |

Este tiempo local (`Ts`) es la referencia para calcular el speedup real del
clúster distribuido en la sección V.

## IV. Virtualización del entorno: Docker

El `Dockerfile` en la raíz construye una imagen `mpi-cluster-image` con
`ubuntu:22.04`, `build-essential`, `openmpi-bin`, `libopenmpi-dev`,
`libssl-dev` y `openssh-server`. Además:

- Habilita `PermitRootLogin yes` en `/etc/ssh/sshd_config`.
- Fija `StrictHostKeyChecking no` en `/etc/ssh/ssh_config`.
- Define `OMPI_ALLOW_RUN_AS_ROOT=1` y `OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1`.
- Compila el binario (`make`) al construir la imagen.

```bash
make docker-build
# equivalente a: docker build -t mpi-cluster-image .
```

### Tabla de validación (ejecutada y verificada el 2026-10-21)

| Validar | Esperado | ¿Funcionó? | Qué se tuvo que modificar o hacer |
| --- | --- | --- | --- |
| `docker run --rm mpi-cluster-image mpicc --version` | Debería mostrar gcc (Ubuntu 11.4.0-1ubuntu1~22.04) | **Sí.** Salida real: `gcc (Ubuntu 11.4.0-1ubuntu1~22.04.3) 11.4.0` | Nada; `mpicc` ya apunta a gcc vía `update-alternatives` al instalar `openmpi-bin`. |
| `docker run --rm mpi-cluster-image mpirun -np 2 echo "Proceso MPI ejecutado"` | Debería imprimir "Proceso MPI ejecutado" 2 veces | **Sí**, pero solo tras agregar `--allow-run-as-root` al comando (`mpirun --allow-run-as-root -np 2 ...`) | Las variables de entorno `OMPI_ALLOW_RUN_AS_ROOT*` habilitan ejecutar como root, pero `mpirun` también exige el flag `--allow-run-as-root` en la línea de comandos; sin él se detiene con error. |
| Crear un contenedor de prueba: `docker run -it --rm mpi-cluster-image /bin/bash` | Se ejecuta y entra a la terminal del contenedor | **Sí** | Nada adicional. |
| `$ service ssh start` | Comienza un service ssh | **Sí.** Salida: `Starting OpenBSD Secure Shell server sshd ... done.` | Nada; `mkdir -p /run/sshd` ya se hace en el Dockerfile (si falta, `sshd` no arranca). |
| `ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa` seguido de `cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys` | Genera una clave y se prueba de forma local | **Sí** | Nada. |
| `ssh localhost echo "Conexion SSH sin contrasena exitosa"` | Imprime el mensaje porque logra levantar una conexión SSH libre | **Sí.** Salida: `Conexion SSH sin contrasena exitosa` (con aviso de host añadido a `known_hosts`, ignorado por `StrictHostKeyChecking no`) | Nada; se automatizó en `scripts/docker/start_node.sh`. |

Los scripts `scripts/docker/start_node.sh` (arranca `sshd` y genera la llave
de cada nodo) y `scripts/docker/trust_worker.sh` (copia la llave del
maestro a un worker) encapsulan estos pasos para no repetirlos a mano en
cada contenedor del clúster físico.

Prueba adicional, con el binario real compilado dentro de la imagen
(`mpirun --allow-run-as-root --oversubscribe -np 4 ./build/bruteforce_mpi`),
confirmando que el programa en sí corre correctamente dentro del
contenedor, en ambos modos:

```
Nodo <hostname>, proceso 0 de 4: rango [0, 16)
Llave encontrada: 23
Tiempo: 0.000102 s
Texto: Texto con la frase objetivo
```

```
Nodo <hostname>, proceso 3 de 4: rango [775000, 800000)
Nodo <hostname>, proceso 3: encontro una candidata en 777777
Llave encontrada: 777777
Tiempo: 0.002500 s
Texto: El equipo rival escondio la palabra secreta aqui.
```

(La segunda prueba usó `--cipher-file` sobre un binario cifrado con DES
real generado aparte, para validar ese modo sin depender todavía del
archivo real de otro equipo.)

## V. Despliegue de red física

_Pendiente de completar en equipo, con el router y las máquinas físicas._

### Paso A: sincronización de llaves SSH inter-nodo

1. En cada contenedor (maestro y workers), levantado con
   `--network host` (ver `make docker-shell` o el comando de la sección
   IV del PDF), correr:
   ```bash
   bash scripts/docker/start_node.sh
   ```
   Esto arranca `sshd`, genera la llave del nodo si no existe, se agrega a
   su propio `authorized_keys` e imprime su IP y su llave pública.
2. Desde el nodo maestro, confiar en cada worker copiando la llave del
   maestro a su `authorized_keys`:
   ```bash
   bash scripts/docker/trust_worker.sh root@<ip_del_worker>
   ```
   (pide la contraseña de root del worker una sola vez, para la copia;
   luego las conexiones son libres).

### Paso B: ejecución del ataque distribuido

1. Copiar `cluster_hosts.example` a `cluster_hosts` y reemplazar las IPs
   por las que el router asignó a cada contenedor, con `slots`/`max_slots`
   según los núcleos físicos de cada máquina.
2. Desde el nodo maestro:
   ```bash
   bash scripts/docker/run_cluster.sh 192.168.1.0/24 cluster_hosts <TOTAL_SLOTS> \
     --cipher-file datos/<archivo_del_otro_equipo>.bin \
     --phrase "<palabra clave objetivo>" \
     --upper 72057594037927936
   ```
   (cambiar `192.168.1.0/24` por la subred real del router).

Evidencia requerida (completar con capturas reales del equipo):

- [ ] Captura del comando `mpirun` ejecutado, con el hostfile visible.
- [ ] Captura de la salida mostrando los distintos nodos (IPs/hostnames)
      participando (el programa ya imprime `Nodo <hostname>, proceso N de
      M: rango [...)` por cada proceso, y `Nodo <hostname>, proceso N:
      encontro una candidata en <llave>` cuando corresponde).
- [ ] Captura del mensaje final `Llave encontrada: ...` con el texto
      recuperado.

## VI. Discusión de resultados y conclusiones

1. **¿Por qué OpenMPI se congela indefinidamente sin `--mca
   btl_tcp_if_include`?**
   Cada contenedor Docker tiene, además de la interfaz física
   (`eth0`/la del host con `--network host`), una interfaz virtual de
   bridge (`docker0`, típicamente en `172.17.0.0/16`). Al arrancar, el
   motor de transporte TCP de OpenMPI (BTL) y el módulo de "out-of-band"
   (OOB, usado para el handshake inicial entre demonios `orted`)
   enumeran **todas** las interfaces de red disponibles y anuncian sus IPs
   a los demás procesos como posibles rutas. Sin restricción, un nodo le
   puede anunciar a otro su IP de `docker0` (172.17.x.x) como si fuera
   alcanzable. El proceso remoto intenta conectarse a esa IP, pero
   172.17.x.x solo existe dentro de la red virtual *local* de cada
   máquina física — no es enrutable a través del router. El `SYN` del
   `connect()` nunca llega a destino ni recibe un `RST` (no hay nadie al
   otro lado que responda "puerto cerrado"), así que el socket se queda
   esperando en el timeout por defecto del kernel, que es largo. Como
   MPI necesita que **todos** los pares completen su conexión antes de
   continuar (barrera implícita del arranque), el programa completo
   parece "colgado" sin ningún mensaje de error. `--mca
   btl_tcp_if_include`/`oob_tcp_if_include 192.168.1.0/24` fuerza a
   OpenMPI a anunciar y usar únicamente la subred física real, eliminando
   la ambigüedad.

2. **Problemas de red enfrentados:**
   _Pendiente de completar en equipo_ (firewall de Windows bloqueando
   puertos TCP dinámicos, necesidad de "Enable Host Networking" en Docker
   Desktop, IPs que cambiaron al reconectar al router, etc.)

3. **Tiempo local vs. clúster físico:**
   _Pendiente de completar en equipo_, comparando `Ts` (sección III) con
   el tiempo medido en la sección V. Nota esperada: la latencia de red y
   el overhead de `--mca`/SSH probablemente hagan que el speedup real sea
   menor al speedup ideal calculado en la Fase 1 con memoria compartida
   local, especialmente si el espacio de búsqueda por nodo es pequeño
   comparado con la latencia de sincronización (`MPI_Allreduce` por lote).

4. **32 procesos MPI en una máquina con 4 núcleos físicos:**
   El sistema operativo igual los deja correr (oversubscription), pero el
   rendimiento se degrada: los 32 procesos compiten por solo 4 núcleos
   lógicos, forzando al *scheduler* del SO a hacer *context switching*
   constante entre ellos. Para un algoritmo como la fuerza bruta DES, que
   es intensivo en CPU sin esperas de E/S, cada cambio de contexto es
   puro overhead (guardar/restaurar registros y caché, sin avanzar
   trabajo útil). Además, los `MPI_Allreduce` por lote obligan a
   sincronizar los 32 procesos en cada ronda: el proceso más lento
   (porque le tocó esperar más turnos de CPU) frena a los demás. El
   resultado típico es que el tiempo total es peor que con 4 procesos, no
   mejor — de ahí la recomendación del enunciado de declarar
   `max_slots`/`max-slots` igual a los núcleos físicos reales.

5. **Vulnerabilidades y retos logísticos de HPC con hardware
   heterogéneo:**
   - **Superficie de ataque ampliada:** `PermitRootLogin yes` y SSH sin
     verificación de huellas (`StrictHostKeyChecking no`) son
     aceptables para un ejercicio de clase en una LAN controlada, pero en
     un clúster real son vulnerabilidades serias: cualquiera en la red
     con acceso a un nodo comprometido puede saltar a los demás como
     root sin fricción (movimiento lateral).
   - **Confianza transitiva frágil:** una vez que las llaves SSH están
     sincronizadas entre todos los nodos, comprometer un solo contenedor
     equivale a comprometer el clúster completo.
   - **Heterogeneidad de hardware:** núcleos, velocidad de CPU y memoria
     distintos entre las máquinas de cada integrante hacen que un
     reparto "parejo" de rango de llaves (como hace `compute_range`) deje
     a los nodos más lentos como cuello de botella, porque los más
     rápidos esperan en la siguiente sincronización.
   - **Dependencia de la red física:** firewalls, NAT de Docker, cambios
     de IP por DHCP y distinta configuración de cada sistema operativo
     (Windows/macOS/Linux) multiplican los puntos de falla fuera del
     control del código.
   - **Sin aislamiento de administración:** administrar varios nodos
     "a mano" (como en este laboratorio) no escala; en producción se
     necesitan herramientas de orquestación (Kubernetes, Ansible, un
     *resource manager* como Slurm) para automatizar lo que aquí se hizo
     manualmente con scripts.

6. **¿Lograron el objetivo? ¿Qué parte gustó más/menos? ¿Qué mejorarían?**
   _Pendiente de completar en equipo_ una vez corrida la prueba física.

---

_Nota: las secciones marcadas "pendiente" dependen de pasos que solo el
equipo puede completar (elegir el texto de otro equipo en Canvas, conectar
las máquinas físicas al router y tomar las capturas de pantalla). El
código, el Dockerfile, los scripts de automatización y la tabla de
validación de la sección IV ya están implementados y probados._
