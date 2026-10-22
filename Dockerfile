# Imagen estandarizada para el clúster MPI de la Parte 2.
# Encapsula el compilador, OpenMPI y SSH para que todos los integrantes
# del equipo corran exactamente el mismo entorno sin importar su sistema
# operativo (Windows, macOS o Linux).
FROM ubuntu:22.04

# Evita que apt pida confirmaciones interactivas durante la instalacion.
ENV DEBIAN_FRONTEND=noninteractive

# Dependencias base: compilador, OpenMPI y el servidor SSH que usan los
# nodos para instanciar procesos remotos.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       openmpi-bin \
       libopenmpi-dev \
       openssh-server \
       libssl-dev \
       iproute2 \
       iputils-ping \
    && rm -rf /var/lib/apt/lists/*

# Permite el acceso root por SSH, requerido para que MPI arranque procesos
# en los nodos trabajadores sin un usuario intermedio.
RUN sed -i 's/#\?PermitRootLogin.*/PermitRootLogin yes/' /etc/ssh/sshd_config \
    && mkdir -p /run/sshd

# Desactiva la verificacion manual de huellas de host: MPI no puede
# detenerse a confirmar "yes" la primera vez que un nodo contacta a otro.
RUN printf 'Host *\n    StrictHostKeyChecking no\n    UserKnownHostsFile /dev/null\n' \
    >> /etc/ssh/ssh_config

# OpenMPI prohibe por defecto correr procesos como root; en un contenedor
# sin usuario adicional esa proteccion no aplica, asi que se anula.
ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

WORKDIR /work
COPY . /work

# Compila el binario MPI una sola vez al construir la imagen.
RUN make clean && make

# Copia los scripts de arranque del clúster (SSH y lanzamiento de mpirun).
RUN chmod +x scripts/docker/*.sh

CMD ["/bin/bash"]
