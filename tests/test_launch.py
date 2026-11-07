#!/usr/bin/env python3
"""Exercise real launch orchestration and history with SDK-compatible stubs."""
from pathlib import Path
from sdk_path import sdk_include, target_flags
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
flags = ['g++', *target_flags(), '-std=c++23', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
         '-Wno-attributes', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
         '-I' + str(root / 'src'), '-I' + str(sdk_include(root))]
with tempfile.TemporaryDirectory() as temporary:
    object_file = Path(temporary) / 'main.o'
    binary = Path(temporary) / 'launch-probe'
    subprocess.run(flags + ['-Dmain=launcher_main', '-c', str(root / 'src/main.cpp'),
                            '-o', str(object_file)], check=True)
    subprocess.run(flags + [str(root / 'tests/launch_probe.cpp'), str(object_file),
                            str(root / 'src/recent.cpp'), str(root / 'src/LazyFile.cpp'),
                            str(root / 'src/strconv.cpp'), '-Wl,--wrap=fopen', '-o', str(binary)], check=True)
    environment = dict(os.environ)
    environment.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
    subprocess.run([str(binary)], check=True, env=environment)
