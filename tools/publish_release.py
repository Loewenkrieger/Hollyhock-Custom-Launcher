#!/usr/bin/env python3
"""Publish the tested assets with the configured stable/prerelease status."""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def release_command(config, tag, assets):
    if tag != config['tag']:
        raise ValueError('Tag does not match tools/release.json')
    if config.get('publish', True) is not True:
        raise ValueError('Publication is disabled for this build')
    prerelease = config.get('prerelease', '-' in config['version'])
    if not isinstance(prerelease, bool) or ('-' in config['version'] and not prerelease):
        raise ValueError('A beta version must be published as a prerelease')
    files = []
    for variant in ['hh2', 'hh3']:
        os_version = config['variants'][variant]['os_version']
        names = ['BUILD-' + variant + '.json', 'SHA256SUMS-' + variant + '.txt',
                 'run-' + variant + '-os' + os_version + '.bin',
                 'hhk' + variant[-1] + '-launcher-' + config['version'] + '-os' + os_version + '.zip']
        for name in names:
            path = assets / name
            if not path.is_file():
                raise ValueError('Missing tested release asset: ' + name)
            files.append(str(path))
    command = ['gh', 'release', 'create', tag, *files, '--verify-tag',
               '--title', 'Hollyhock Launcher ' + tag, '--notes-file', 'docs/RELEASE.md']
    if config.get('release_branch'):
        command += ['--target', config['release_branch']]
    if prerelease:
        command += ['--prerelease', '--latest=false']
    return command


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tag', required=True)
    parser.add_argument('--assets', type=Path, default=Path('release'))
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    config = json.loads((ROOT / 'tools/release.json').read_text(encoding='utf-8'))
    try:
        command = release_command(config, args.tag, args.assets)
    except ValueError as error:
        parser.error(str(error))
    if args.dry_run:
        print(json.dumps(command))
    else:
        subprocess.run(command, cwd=ROOT, check=True)


if __name__ == '__main__':
    main()
