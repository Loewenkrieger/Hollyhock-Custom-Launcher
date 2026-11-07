#!/usr/bin/env python3
"""Exercise the real exit-result factory and the firmware's narrow consumer ABI.

The host test maps the OS code probe at its real address, so it uses UBSan;
that address overlaps AddressSanitizer's shadow area on x86-64.
The optional target test interprets the actual SH caller and compiled callback.
It is not an emulator for the complete Casio OS or calculator hardware.
"""
from pathlib import Path
from sdk_path import sdk_include, target_flags
import argparse
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--target-elf', type=Path)
parser.add_argument('--target-bin', type=Path)
parser.add_argument('--nm', default='sh4a_nofpueb-elf-nm')
args = parser.parse_args()
sdk = sdk_include(root)
with tempfile.TemporaryDirectory() as temporary:
    binary = Path(temporary) / 'exit-probe'
    subprocess.run(['g++', *target_flags(), '-std=c++23', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-attributes',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    '-I' + str(root/'src'), '-I' + str(sdk), str(root/'tests/exit_probe.cpp'),
                    str(root/'src/exit_protocol.cpp'), str(root/'src/exit_result.cpp'),
                    str(root/'src/strconv.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

if bool(args.target_elf) != bool(args.target_bin):
    parser.error('--target-elf and --target-bin must be supplied together')
if args.target_elf:
    import exit_sh_consumer
    exit_sh_consumer.test(args.target_elf, args.target_bin, args.nm)
