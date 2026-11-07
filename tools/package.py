#!/usr/bin/env python3
"""Create the install bundle with complete pinned SDK sources and checksums."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import zipfile
from source import ROOT, inventory, test_input_hash

parser = argparse.ArgumentParser()
parser.add_argument('--variant',choices=['hh2','hh3'],required=True)
parser.add_argument('--outdir', type=Path, default=ROOT / 'dist')
args = parser.parse_args()
out = args.outdir.resolve()
config = json.loads((ROOT / 'tools/release.json').read_text())
variant=config["variants"][args.variant]
version=variant["os_version"]
info = inventory()
assert info['sdk_commit'] == config['upstream']['hhk3_sdk_commit'], 'Unexpected SDK revision'
inputs = test_input_hash()
reports = {}
for suite in ['host', 'target']:
    report = json.loads((out / ('tests-' + suite + '.json')).read_text())
    assert report['passed'] and report['variant'] == args.variant and report['inputs_sha256'] == inputs, 'Missing or stale ' + suite + ' tests'
    reports[suite] = report
run = (out / ('run.' + version + '.bin')).read_bytes()
run_hash = hashlib.sha256(run).hexdigest()
assert reports['target']['run_bin_sha256'] == run_hash, 'Target tests belong to a different binary'
elf = (out / ('run.' + version + '.elf')).read_bytes()
assert elf[:6] == b'\x7fELF\x01\x02', 'Expected big-endian ELF32'
assert struct.unpack_from('>H', elf, 18)[0] == 42, 'Expected SH ELF'
assert struct.unpack_from('>I', elf, 24)[0] == 0x8cfd0000, 'Unexpected launcher entry point'
assert 0 < len(run) < 0x20000, 'Launcher exceeds reserved RAM'
compiler = subprocess.check_output(['sh4a_nofpueb-elf-g++', '--version'], text=True).splitlines()[0]
metadata = dict({k:v for k,v in config.items() if k != "variants"}, **variant, variant=args.variant, source_commit=info['commit'], source_dirty=info['dirty'],
                compiler=compiler, inputs_sha256=inputs, run_bin_size=len(run),
                run_bin_sha256=run_hash, tests={'host': 'passed', 'target': 'passed'})
metadata['history']=dict(config['history'],variable=variant['history_variable'])
release = out.parent / 'release'
release.mkdir(parents=True, exist_ok=True)
bundle_name = ('hhk2' if args.variant == 'hh2' else 'hhk3') + '-launcher-' + config['version'] + '-os' + version
run_name = 'run-' + args.variant + '-os' + version + '.bin'
metadata_name = 'BUILD-' + args.variant + '.json'
entries = {'run.bin': run, 'BUILD.json': (json.dumps(metadata, indent=2) + '\n').encode(),
           'debug/run.' + version + '.elf': elf}
for extension in (['hhk','bin'] if args.variant == 'hh2' else ['hh3']):
    entries['smoke/launcher-test.' + extension] = (ROOT / ('smoke/' + args.variant + '/launcher-test.' + extension)).read_bytes()
for name in ['README_DE.md', 'TEST-REPORT.txt', 'PERFORMANCE.md']:
    entries[name] = (ROOT / 'docs' / name).read_bytes()
entries['README_DE.md'] = entries['README_DE.md'].replace(b'(BUILD.md)', b'(source/docs/BUILD.md)')
# Upstream notices are always included with the SDK sources below.
if (ROOT / 'LICENSE').is_file():
    entries['LICENSE'] = (ROOT / 'LICENSE').read_bytes()
for path in (ROOT / 'docs/preview').glob('*.png'):
    entries['preview/' + path.name] = path.read_bytes()
for name in info['files']:
    entries['source/' + name] = (ROOT / name).read_bytes()
entries['source/SOURCE-INFO.json'] = (json.dumps(info, indent=2) + '\n').encode()
for suite in reports:
    entries['tests-' + suite + '.json'] = (out / ('tests-' + suite + '.json')).read_bytes()


def checksums(files):
    return ''.join(hashlib.sha256(data).hexdigest() + '  ' + name + '\n'
                   for name, data in sorted(files.items())).encode()


entries['SHA256SUMS.txt'] = checksums(entries)
archive = release / (bundle_name + '.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as package:
    for name, data in sorted(entries.items()):
        item = zipfile.ZipInfo(bundle_name + '/' + name, date_time=(2026, 1, 1, 0, 0, 0))
        item.compress_type = zipfile.ZIP_DEFLATED
        item.external_attr = 0o100644 << 16
        package.writestr(item, data)
with zipfile.ZipFile(archive) as package:
    assert package.testzip() is None, 'ZIP CRC check failed'
    assert len(package.namelist()) == len(entries), 'Duplicate ZIP entries'
shutil.copyfile(out / ('run.' + version + '.bin'), release / run_name)
(release / metadata_name).write_bytes(entries['BUILD.json'])
assets = {name: (release / name).read_bytes()
          for name in [run_name, metadata_name, archive.name]}
(release / ('SHA256SUMS-' + args.variant + '.txt')).write_bytes(checksums(assets))
print(f'Package verified: {archive} ({len(entries)} files, complete SDK source included)')
