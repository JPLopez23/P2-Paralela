#!/usr/bin/env python3
"""Convierte el texto cifrado en base64 (publicado en el foro de Canvas por
otro equipo) al archivo binario que espera --cipher-file.

Uso:
    python3 scripts/decodificar_cifrado.py "<base64>" datos/texto_otro_equipo.bin
    # o por stdin:
    echo "<base64>" | python3 scripts/decodificar_cifrado.py - datos/texto_otro_equipo.bin
"""
import base64
import sys

DES_BLOCK_SIZE = 8


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Uso: {sys.argv[0]} <base64|-> <archivo_salida.bin>", file=sys.stderr)
        return 1

    raw = sys.stdin.read().strip() if sys.argv[1] == "-" else sys.argv[1].strip()

    try:
        data = base64.b64decode(raw, validate=True)
    except Exception as exc:
        print(f"Error: base64 invalido o incompleto ({exc}).", file=sys.stderr)
        print("Revisa que copiaste el texto completo del foro de Canvas.", file=sys.stderr)
        return 1

    if len(data) == 0 or len(data) % DES_BLOCK_SIZE != 0:
        print(
            f"Advertencia: {len(data)} bytes no es multiplo de {DES_BLOCK_SIZE} "
            "(tamano de bloque DES). Revisa que el base64 este completo.",
            file=sys.stderr,
        )
        return 1

    out_path = sys.argv[2]
    with open(out_path, "wb") as f:
        f.write(data)

    print(f"OK: {len(data)} bytes escritos en {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
