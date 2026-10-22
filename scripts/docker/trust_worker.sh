#!/bin/bash
# Se corre en el nodo MAESTRO para confiar en un nodo trabajador: copia la
# llave publica del maestro al authorized_keys del worker indicado.
# Uso: trust_worker.sh usuario@ip_del_worker
set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Uso: $0 root@ip_del_worker" >&2
    exit 1
fi

WORKER="$1"

if [ ! -f ~/.ssh/id_rsa.pub ]; then
    echo "Corra primero start_node.sh en este contenedor." >&2
    exit 1
fi

ssh-copy-id -o StrictHostKeyChecking=no -i ~/.ssh/id_rsa.pub "$WORKER"
echo "Probando conexion sin contrasena con $WORKER..."
ssh -o StrictHostKeyChecking=no "$WORKER" echo "Conexion SSH sin contrasena exitosa desde el maestro"
