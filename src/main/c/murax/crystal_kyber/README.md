This directory is a bare-metal Murax firmware target for integrating Crystal-Kyber.

Build:

```sh
make
```

Artifacts:

- `build/crystal_kyber.elf`
- `build/crystal_kyber.hex`
- `build/crystal_kyber.asm`

The Murax simulation target `vexriscv.demo.MuraxCrystalKyberWithRamInit` preloads `build/crystal_kyber.hex` into on-chip RAM.

Notes:

- This scaffold builds on plain `rv32i` by default, matching the default Murax CPU configuration.
- If you switch the CPU to an `M`-enabled configuration later, you can build firmware with `make MULDIV=yes`.
- The current `main.c` is a harness and placeholder. Drop your Crystal-Kyber sources into `src/` and call them from `main()`.
