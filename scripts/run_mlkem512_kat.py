#!/usr/bin/env python3
import argparse
import time
import subprocess
from pathlib import Path

EXPECTED_FIELDS = {
    "keypair_ret": "0x00000000",
    "enc_ret": "0x00000000",
    "dec_ret": "0x00000000",
    "pk_match": "0x00000001",
    "sk_match": "0x00000001",
    "ct_match": "0x00000001",
    "ss_match": "0x00000001",
    "kat_pass": "0x00000001",
}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def run_command(cmd: list[str], cwd: Path, timeout: int | None = None) -> None:
    print(f"$ {' '.join(cmd)}")
    proc = subprocess.run(cmd, cwd=cwd, timeout=timeout)
    if proc.returncode != 0:
        raise SystemExit(proc.returncode)


def generate_reference_vectors(root: Path) -> None:
    generator = root / "scripts" / "generate_mlkem512_kat_vectors.py"
    run_command(["python3", str(generator)], cwd=root)


def run_verilator_until_done(sim_dir: Path, log_path: Path, timeout: int) -> str:
    vmurax = sim_dir / "obj_dir" / "VMurax"
    if not vmurax.exists():
        raise SystemExit(f"Simulator not found: {vmurax}")

    log_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"$ {vmurax} > {log_path}  # stops after done")

    proc = subprocess.Popen(
        [str(vmurax)],
        cwd=sim_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    lines: list[str] = []
    start_time = time.monotonic()
    try:
        with log_path.open("w", encoding="utf-8") as log_file:
            while True:
                if time.monotonic() - start_time > timeout:
                    proc.kill()
                    proc.wait()
                    raise SystemExit(f"Simulation timeout after {timeout}s. Log: {log_path}")

                line = proc.stdout.readline() if proc.stdout is not None else ""
                if line:
                    print(line, end="")
                    log_file.write(line)
                    log_file.flush()
                    lines.append(line)
                    if line.strip() == "done":
                        proc.terminate()
                        break
                elif proc.poll() is not None:
                    break

        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
    except Exception:
        proc.kill()
        proc.wait()
        raise

    output = "".join(lines)
    if "done" not in output:
        raise SystemExit(f"Simulation did not reach done within expected flow. Log: {log_path}")

    return output


def parse_key_values(output: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in output.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()
    return values


def validate_output(output: str) -> None:
    values = parse_key_values(output)
    failures = []

    for key, expected in EXPECTED_FIELDS.items():
        actual = values.get(key)
        if actual != expected:
            failures.append((key, expected, actual))

    if failures:
        print("KAT validation failed:")
        for key, expected, actual in failures:
            print(f"  {key}: expected {expected}, got {actual}")
        raise SystemExit(1)

    print("KAT validation passed: kat_pass=0x00000001")


def main() -> int:
    root = repo_root()
    parser = argparse.ArgumentParser(description="Build and run ML-KEM-512 KAT on Murax/VexRiscv.")
    parser.add_argument(
        "--skip-reference",
        action="store_true",
        help="Do not regenerate/check kat_vectors.h from the mlkem-native submodule.",
    )
    parser.add_argument("--skip-build", action="store_true", help="Run only the existing Verilator binary.")
    parser.add_argument(
        "--log",
        default=str(root / "understanding" / "benchmarks" / "mlkem512_kat.log"),
        help="Path where the Verilator UART log will be written.",
    )
    parser.add_argument("--timeout", type=int, default=180, help="Reserved timeout value in seconds.")
    args = parser.parse_args()

    firmware_dir = root / "src" / "main" / "c" / "murax" / "crystal_kyber"
    sim_dir = root / "src" / "test" / "cpp" / "murax"

    if not args.skip_reference:
        generate_reference_vectors(root)

    if not args.skip_build:
        run_command(["make", "-B", "-C", str(firmware_dir), "KAT=yes", "all"], cwd=root)
        run_command(["sbt", "runMain vexriscv.demo.MuraxCrystalKyberWithRamInit"], cwd=root)
        run_command(["make", "-B", "-C", str(sim_dir), "compile"], cwd=root)

    output = run_verilator_until_done(sim_dir, Path(args.log), args.timeout)
    validate_output(output)
    print(f"log={Path(args.log).resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
