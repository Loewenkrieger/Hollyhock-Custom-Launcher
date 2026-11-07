#!/usr/bin/env python3
"""Run the host or target checks and save their actual output for packaging."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
from source import test_input_hash
import hashlib

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('suite', choices=['host', 'target'])
parser.add_argument('--variant', choices=['hh2','hh3'], default='hh3')
parser.add_argument('--outdir', type=Path, default=root / 'dist')
args = parser.parse_args()
import os
os.environ["LAUNCHER_TARGET"] = args.variant
version = "2000" if args.variant == "hh2" else "7002"
commands = [[sys.executable, 'tests/test_' + name + '.py'] for name in
            ['classic', 'discovery', 'hh2' if args.variant == 'hh2' else 'loader', 'recent', 'launch', 'exit']]
commands[0] += ['--preview', str(args.outdir / 'preview')]
if args.suite == 'target':
    commands = [[sys.executable, 'tests/test_mcs_abi.py'],
                [sys.executable, '-c',
                 'import sys; from pathlib import Path; sys.path.insert(0,"tests"); '
                 'import exit_sh_consumer; exit_sh_consumer.test('
                 'Path(sys.argv[1]), Path(sys.argv[2]), "sh4a_nofpueb-elf-nm")',
                 str(args.outdir / 'run.7002.elf'), str(args.outdir / ('run.' + version + '.bin'))]]
    if args.variant == 'hh2': commands = commands[:1]
    commands.append([sys.executable, 'tests/test_target.py', '--variant', args.variant, '--outdir', str(args.outdir)])
args.outdir.mkdir(parents=True, exist_ok=True)
report = args.outdir / ('tests-' + args.suite + '.json')
# A failed rerun must not leave a previous success report available to package.
report.unlink(missing_ok=True)
inputs = test_input_hash()
results = []
for command in commands:
    process = subprocess.run(command, cwd=root, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, text=True)
    print(process.stdout, end='', flush=True)
    results.append({'command': command[1:], 'exit_code': process.returncode,
                    'output': process.stdout})
    if process.returncode:
        raise SystemExit(process.returncode)
assert inputs == test_input_hash(), 'Test inputs changed during the run'
summary = {'variant': args.variant, 'suite': args.suite, 'passed': True, 'inputs_sha256': inputs, 'checks': results}
if args.suite == 'target':
    summary['run_bin_sha256'] = hashlib.sha256((args.outdir / ('run.' + version + '.bin')).read_bytes()).hexdigest()
report.write_text(json.dumps(summary,
                             indent=2) + '\n', encoding='utf-8')
print(f'{args.suite} suite passed; report: {report}')
