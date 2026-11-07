#!/usr/bin/env python3
"""Exercise the production history code with SDK-compatible MCS stubs."""
from pathlib import Path
from sdk_path import sdk_include, target_flags
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temporary:
    binary = Path(temporary) / 'recent-probe'
    subprocess.run(['g++', *target_flags(), '-std=c++23', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-attributes', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I' + str(root / 'src'), '-I' + str(sdk_include(root)),
                    str(root / 'tests/recent_probe.cpp'), str(root / 'src/recent.cpp'),
                    str(root / 'src/LazyFile.cpp'),
                    '-o', str(binary)], check=True)
    environment = dict(os.environ)
    environment.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
    subprocess.run([str(binary)], check=True, env=environment)
