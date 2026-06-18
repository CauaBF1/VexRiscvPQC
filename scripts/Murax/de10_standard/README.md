# Murax ML-KEM-512 na DE10-Standard

Este alvo executa o firmware na FPGA Cyclone V da DE10-Standard usando apenas os LEDs.

## Significado dos LEDs

- `0001`: firmware iniciou.
- `0010`: keypair em execução.
- `0100`: encapsulation em execução.
- `1000`: decapsulation em execução ou falha final.
- `1111`: execução concluída com sucesso.

Os bits são mostrados em `LEDR[3:0]`. `KEY0` (botão físico KEY[0]) reinicia o SoC.

## Benchmark normal

```bash
make -C scripts/Murax/de10_standard build \
  RISCV_PATH=$HOME/opt/riscv-elf-multilib
make -C scripts/Murax/de10_standard program
```

## KAT

```bash
make -C scripts/Murax/de10_standard kat-program \
  RISCV_PATH=$HOME/opt/riscv-elf-multilib
```

No KAT, `LEDR[3:0] = 1111` significa que `pk`, `sk`, `ct` e `ss` coincidiram com o vetor de referência.

Se a toolchain estiver no `PATH`, use `RISCV_PATH=`. Para listar o cabo Quartus, execute `jtagconfig`.

## Permissao do USB-Blaster

A interface observada nesta placa usa `09fb:6010`. Configure a regra udev:

```bash
sudo tee /etc/udev/rules.d/51-usbblaster.rules >/dev/null <<'RULE'
SUBSYSTEM=="usb", ATTR{idVendor}=="09fb", ATTR{idProduct}=="6010", MODE="0660", GROUP="plugdev", TAG+="uaccess"
RULE
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Desconecte e reconecte o cabo USB-Blaster. Confirme com `jtagconfig` antes de executar `make program`.

Na cadeia JTAG da DE10-Standard, o HPS aparece como dispositivo 1 e o FPGA como dispositivo 2. O Makefile coloca `SOCVHPS@1` em bypass e programa o FPGA em `FPGA_DEVICE_INDEX=2`.
