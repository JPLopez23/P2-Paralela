#!/bin/bash
# Arranca el demonio SSH de este contenedor y prepara su propia llave,
# para que tanto el nodo maestro como cada trabajador puedan recibir
# conexiones entrantes de MPI. Se corre una vez dentro de cada contenedor
# (maestro y workers) despues de "docker run -it --network host ...".
set -euo pipefail

service ssh start

mkdir -p ~/.ssh
chmod 700 ~/.ssh

if [ ! -f ~/.ssh/id_rsa ]; then
    ssh-keygen -t rsa -N "" -f ~/.ssh/id_rsa
fi

# Permite que este nodo se conecte a si mismo (requisito de MPI incluso
# para el proceso donde corre el rank maestro).
touch ~/.ssh/authorized_keys
if ! grep -qf ~/.ssh/id_rsa.pub ~/.ssh/authorized_keys 2>/dev/null; then
    cat ~/.ssh/id_rsa.pub >> ~/.ssh/authorized_keys
fi
chmod 600 ~/.ssh/authorized_keys

echo "Nodo listo. IP(s) de este contenedor:"
hostname -I

echo "Llave publica de este nodo (copiarla al authorized_keys de los otros):"
cat ~/.ssh/id_rsa.pub
