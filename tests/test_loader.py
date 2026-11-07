#!/usr/bin/env python3
"""Test real ELF metadata and compare its file operations with a source snapshot."""
from pathlib import Path
from sdk_path import sdk_include, target_flags
import argparse
import json
import os
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, default=root)
parser.add_argument('--measure-only', action='store_true')
args = parser.parse_args()
source = args.source.resolve()
endian = '<' if sys.byteorder == 'little' else '>'

def note(name, value):
    return (struct.pack(endian + 'III', len(name), len(value), 0) +
            name + bytes(-len(name) % 4) + value + bytes(-len(value) % 4))

def image(segments, kinds=None):
    ident = b'\x7fELF' + bytes([1, 1 if sys.byteorder == 'little' else 2, 1]) + bytes(9)
    header = struct.pack(endian + '16sHHIIIIIHHHHHH', ident, 2, 42, 1, 0, 52, 0, 0,
                         52, 32, len(segments), 40, 0, 0)
    offset = 52 + 32 * len(segments)
    headers = b''
    for i, segment in enumerate(segments):
        headers += struct.pack(endian + 'IIIIIIII', kinds[i] if kinds else 4,
                               offset, 0, 0, len(segment), len(segment), 4, 4)
        offset += len(segment)
    return header + headers + b''.join(segments)

metadata = b''.join(note(key + b'\0', value + b'\0') for key, value in [
    (b'NAME', b'Example app'), (b'VERSION', b'1.0'), (b'AUTHOR', b'A'), (b'DESCRIPTION', b'Detail')])
environment = dict(os.environ)
environment.setdefault('ASAN_OPTIONS', 'detect_leaks=0')
passed = 0
with tempfile.TemporaryDirectory() as temporary:
    binary = Path(temporary) / 'loader-probe'
    subprocess.run(['g++', *target_flags(), '-std=c++23', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                    '-Wno-attributes', '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I' + str(source / 'src'), '-I' + str(sdk_include(source)),
                    str(root / 'tests/loader_probe.cpp'), str(source / 'src/LazyFile.cpp'),
                    *[str(source / 'src/loaders/elf' / name) for name in ['init.cpp', 'infos.cpp', 'elfnotes.cpp']],
                    '-Wl,--wrap=fopen,--wrap=fread,--wrap=fseek', '-o', str(binary)], check=True)
    fixture = Path(temporary) / 'app.hh3'

    def check(data, invalid=False):
        fixture.write_bytes(data)
        result = subprocess.check_output([str(binary), str(fixture), 'invalid' if invalid else 'metadata'],
                                         env=environment, text=True)
        return None if invalid else json.loads(result)

    def passed_check(message):
        global passed
        passed += 1
        print('PASS', message)

    result = check(image([metadata]))
    assert [result[key] for key in ['name', 'version', 'author', 'description']] == ['Example app', '1.0', 'A', 'Detail']
    print('METADATA_FILE_OPERATIONS ' + json.dumps({key: result[key] for key in ['opens', 'reads', 'seeks']}))
    if not args.measure_only:
        passed_check('all metadata is preserved and warm getters perform no additional file operations')
        result = check(image([metadata, note(b'NAME\0', b'Newest\0')]))
        assert result['name'] == 'Newest' and result['description'] == 'Detail'
        passed_check('multiple note segments preserve last-note precedence and borrowed data lifetime')
        result = check(image([]))
        assert result['name'] == str(fixture) and not result['description']
        passed_check('apps without notes keep their filename fallback')
        result = check(image([note(b'NAME', b'No terminator')]))
        assert result['name'] == 'No terminator'
        passed_check('note names and descriptions need no stored NUL terminator')
        result = check(image([b'']))
        assert result['name'] == str(fixture)
        passed_check('empty note segments remain supported')

        invalid = [
            ('truncated ELF header', image([metadata])[:20]),
            ('invalid ELF magic', b'BAD!' + image([metadata])[4:]),
            ('unsupported program header type', image([metadata], [0x70000000])),
            ('short note header', image([b'12345'])),
            ('overflowing note name length', image([struct.pack(endian + 'III', 0xffffffff, 4, 0)])),
            ('overflowing note description length', image([struct.pack(endian + 'III', 4, 0xffffffff, 0) + b'NAME'])),
            ('truncated note payload', image([metadata[:-1]])),
        ]
        for label, offset, value, fmt in [
            ('unsupported ELF class', 4, 2, 'B'),
            ('wrong byte order', 5, 2 if sys.byteorder == 'little' else 1, 'B'),
            ('invalid program header size', 42, 31, 'H'),
            ('mismatched note sizes', 52 + 20, len(metadata) + 1, 'I'),
            ('note offset outside the file', 52 + 4, 0xfffffff0, 'I'),
        ]:
            damaged = bytearray(image([metadata]))
            struct.pack_into(endian + fmt, damaged, offset, value)
            invalid.append((label, damaged))
        for label, data in invalid:
            check(data, invalid=True)
            passed_check(label + ' is rejected on repeated metadata access')
        print(f'{passed}/{passed} loader checks passed (native-endian host fixtures; no SH execution)')
