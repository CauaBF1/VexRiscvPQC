#!/usr/bin/env bash
set -euo pipefail

if [ $# -ne 1 ]; then
  echo "usage: $0 /path/to/VexRiscvPQC/litex" >&2
  exit 1
fi

LITEX_DIR=$1
BIOS_DIR="$LITEX_DIR/litex/litex/soc/software/bios"
PKG_DIR=$(cd "$(dirname "$0")/.." && pwd)

if [ ! -d "$BIOS_DIR" ]; then
  echo "BIOS directory not found: $BIOS_DIR" >&2
  exit 1
fi

cp "$PKG_DIR/bios/Makefile" "$BIOS_DIR/Makefile"
cp "$PKG_DIR/bios/mlkem_litex.c" "$BIOS_DIR/mlkem_litex.c"
cp "$PKG_DIR/bios/mlkem_native_litex_config.h" "$BIOS_DIR/mlkem_native_litex_config.h"
cp "$PKG_DIR/bios/kat_vectors.h" "$BIOS_DIR/kat_vectors.h"

if ! grep -q 'litex_mlkem_kat_status' "$BIOS_DIR/main.c"; then
  echo "main.c still needs the ML-KEM call. Apply patches/main.c.patch manually." >&2
  exit 2
fi

echo "Installed ML-KEM LiteX BIOS files into $BIOS_DIR"
