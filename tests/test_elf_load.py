#!/usr/bin/env python3
"""Exercise actual ELF segment loading and entry-call ABI with a host fixture."""
from pathlib import Path
from sdk_path import sdk_include, target_flags
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temporary:
    binary = Path(temporary)/'elf-load'
    sources = ['tests/elf_load_probe.cpp','src/LazyFile.cpp',
               'src/loaders/elf/init.cpp','src/loaders/elf/infos.cpp',
               'src/loaders/elf/elfnotes.cpp','src/loaders/elf/loading.cpp']
    subprocess.run(['g++',*target_flags(),'-std=c++23','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-attributes','-fsanitize=undefined','-fno-sanitize-recover=all',
                    '-I'+str(root/'src'),'-I'+str(sdk_include(root)),
                    *[str(root/name) for name in sources],'-o',str(binary)],check=True)
    subprocess.run([str(binary),str(Path(temporary)/'Mixed.Hh3')],check=True)
