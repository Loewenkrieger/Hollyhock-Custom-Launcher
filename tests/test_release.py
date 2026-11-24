#!/usr/bin/env python3
"""Check publication mode and validation without calling GitHub or gh."""
from copy import deepcopy
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from publish_release import release_command


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.assets = Path(self.directory.name)
        self.config = {'version': '0.7.0-beta.1', 'tag': 'v0.7.0-beta.1',
                       'publish': True, 'prerelease': True, 'release_branch': 'codex/design-beta',
                       'variants': {'hh2': {'os_version': '2000'}, 'hh3': {'os_version': '7002'}}}
        for variant, version in [('hh2', '2000'), ('hh3', '7002')]:
            for name in ['BUILD-' + variant + '.json', 'SHA256SUMS-' + variant + '.txt',
                         'run-' + variant + '-os' + version + '.bin',
                         'hhk' + variant[-1] + '-launcher-0.7.0-beta.1-os' + version + '.zip']:
                (self.assets / name).write_bytes(b'fixture')

    def test_beta_is_prerelease_and_never_latest(self):
        command = release_command(self.config, self.config['tag'], self.assets)
        self.assertIn('--prerelease', command)
        self.assertIn('--latest=false', command)
        self.assertIn('--verify-tag', command)
        self.assertEqual(command[command.index('--target') + 1], 'codex/design-beta')
        self.assertTrue(all(Path(arg).is_file() for arg in command[4:12]))
        self.assertEqual(command[12], '--verify-tag')

    def test_stable_does_not_inherit_beta_flags(self):
        config = deepcopy(self.config)
        config.update(version='0.7.0', tag='v0.7.0', prerelease=False)
        for path in list(self.assets.glob('*-0.7.0-beta.1-*.zip')):
            path.with_name(path.name.replace('0.7.0-beta.1', '0.7.0')).write_bytes(path.read_bytes())
        command = release_command(config, config['tag'], self.assets)
        self.assertNotIn('--prerelease', command)
        self.assertNotIn('--latest=false', command)

    def test_beta_cannot_be_marked_stable(self):
        self.config['prerelease'] = False
        with self.assertRaises(ValueError):
            release_command(self.config, self.config['tag'], self.assets)

    def test_disabled_publication_is_rejected(self):
        self.config['publish'] = False
        with self.assertRaises(ValueError):
            release_command(self.config, self.config['tag'], self.assets)

    def test_wrong_tag_is_rejected(self):
        with self.assertRaises(ValueError):
            release_command(self.config, 'v0.6.2', self.assets)

    def test_missing_asset_is_rejected(self):
        (self.assets / 'run-hh2-os2000.bin').unlink()
        with self.assertRaises(ValueError):
            release_command(self.config, self.config['tag'], self.assets)


if __name__ == '__main__':
    unittest.main(verbosity=2)
