#!/usr/bin/env python3
"""Exercise the exact UI controller/renderer; does not emulate the OS/hardware."""
from pathlib import Path
from sdk_path import sdk_include, target_flags
import argparse
import subprocess
import tempfile
import os

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--preview', type=Path)
args = parser.parse_args()
with tempfile.TemporaryDirectory() as temporary:
    binary = Path(temporary) / 'classic-probe'
    subprocess.run(['g++', *target_flags(), '-std=c++23', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I' + str(root / 'src'), str(root / 'tests/classic_probe.cpp'),
                    str(root / 'src/classic/ui.cpp'), '-o', str(binary)], check=True)
    command = [str(binary)]
    if args.preview:
        args.preview.mkdir(parents=True, exist_ok=True)
        command.append(str(args.preview.resolve()))
    # LeakSanitizer cannot inspect /proc threads in some hosted environments.
    # Address/undefined-behavior instrumentation remains enabled.
    environment = dict(os.environ)
    environment.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
    subprocess.run(command, check=True, env=environment)
    adapter = Path(temporary) / 'classic-adapter'
    subprocess.run(['g++', *target_flags(), '-std=c++23', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-Wno-attributes',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I' + str(root / 'src'), '-I' + str(sdk_include(root)),
                    str(root / 'tests/classic_adapter.cpp'), str(root / 'src/gui.cpp'),
                    str(root / 'src/classic/ui.cpp'), '-o', str(adapter)], check=True)
    subprocess.run([str(adapter)], check=True, env=environment)
