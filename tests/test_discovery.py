#!/usr/bin/env python3
"""Test actual HH3-only app discovery with a mock calculator file system."""
from pathlib import Path
from sdk_path import sdk_include, target_flags
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temporary:
    binary = Path(temporary) / 'discovery-probe'
    subprocess.run(['g++', *target_flags(), '-std=c++23', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-attributes', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I' + str(root / 'src'), '-I' + str(sdk_include(root)),
                    str(root / 'tests/discovery_probe.cpp'), str(root / 'src/app_list.cpp'),
                    str(root / 'src/LazyFile.cpp'),
                    *([str(root / 'src/loaders/hh2/loader.cpp'), str(root / 'src/loaders/hh2/image.cpp')] if os.environ.get('LAUNCHER_TARGET') == 'hh2' else []), '-o', str(binary)], check=True)
    environment = dict(os.environ)
    environment.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
    subprocess.run([str(binary)], check=True, env=environment)
