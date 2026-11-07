"""Locate the pinned SDK, including older snapshots used for measurements."""
import os
from pathlib import Path


def sdk_include(root):
    candidates = [root / 'meta/hollyhock-3/sdk', root / 'sdk']
    if os.environ.get('SDK_DIR'):
        candidates.insert(0, Path(os.environ['SDK_DIR']))
    for sdk in candidates:
        if (sdk / 'include/sdk/os/mcs.h').is_file():
            return sdk / 'include'
    raise SystemExit('SDK missing: run git submodule update --init --recursive')


def target_flags():
    return ["-DLAUNCHER_HH2=" + ("1" if os.environ.get("LAUNCHER_TARGET") == "hh2" else "0")]
