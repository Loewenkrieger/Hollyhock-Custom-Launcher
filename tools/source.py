"""Source inventory and test-input identity for checkouts and release snapshots."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MODULE = 'meta/hollyhock-3'
ADDRESSES = [MODULE + '/versions/v' + os + '/addresses.json' for os in ['02.01.2000.0000','02.01.7002.0001']]


def git(*args, cwd=ROOT):
    return subprocess.check_output(['git', *args], cwd=cwd).decode().strip()


def inventory():
    if not (ROOT / '.git').exists():
        return json.loads((ROOT / 'SOURCE-INFO.json').read_text())
    names = git('ls-files', '--cached', '--others', '--exclude-standard', '-z').split('\0')
    files = {name for name in names if name and (ROOT / name).is_file()
             and not name.startswith(('downloads/', 'sdk/'))}
    module_names = git('ls-files', '-z', cwd=ROOT / MODULE).split('\0')
    files.update(MODULE + '/' + name for name in module_names
                 if name.startswith('sdk/') or name == 'LICENSE')
    files.update(ADDRESSES)
    return {'commit': git('rev-parse', 'HEAD'),
            'sdk_commit': git('rev-parse', 'HEAD', cwd=ROOT / MODULE),
            'dirty': bool(git('status', '--porcelain', '--untracked-files=normal')),
            'files': sorted(files)}


def test_input_hash():
    paths = [ROOT / name for name in ['Makefile', 'linker.ld', 'custom-start.specs', 'convert_json.py']]
    for directory in ['src', 'tests', 'smoke', 'tools', MODULE + '/sdk']:
        paths.extend(path for path in (ROOT / directory).rglob('*')
                     if path.is_file() and (path.suffix in ['.cpp', '.hpp', '.h', '.c', '.S', '.py', '.ld', '.def', '.json']
                                            or path.name == 'Makefile'))
    paths.extend(ROOT / address for address in ADDRESSES)
    digest = hashlib.sha256()
    for path in sorted(set(paths)):
        digest.update(path.relative_to(ROOT).as_posix().encode() + b'\0' + path.read_bytes() + b'\0')
    return digest.hexdigest()
