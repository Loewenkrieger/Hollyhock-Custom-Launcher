#!/usr/bin/env python3
"""Verify real SDK rebinding using each target's pinned OS addresses."""
from pathlib import Path
import ctypes
import json
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
variant = os.environ.get('LAUNCHER_TARGET','hh3')
full = '02.01.2000.0000' if variant == 'hh2' else '02.01.7002.0001'
addresses = json.loads((root/('meta/hollyhock-3/versions/v'+full+'/addresses.json')).read_text())
values = {entry['FunctionName']:int(entry['startAddress'],16) for entry in addresses['addresses']}
width = ctypes.sizeof(ctypes.c_void_p)
table = addresses['safeGuard'].encode()+b'\0'
for name,address in values.items():
    table += name.encode()+b'\0'+address.to_bytes(width,'big')
environment = dict(os.environ)
environment.pop('HHK_SYMBOL_TABLE',None)
environment.pop('HHK_SYMBOL_TABLE_LEN',None)
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    (tmp/'symbols').write_bytes(table)
    subprocess.run(['g++','-std=c++23','-O1','-g','-Wall','-Wextra','-Werror',
                    '-Wno-prio-ctor-dtor','-fsanitize=undefined','-fno-sanitize-recover=all',
                    '-I'+str(root/'meta/hollyhock-3/sdk/include'),
                    str(root/'tests/sdk_relink_probe.cpp'),
                    str(root/'meta/hollyhock-3/sdk/os/functions/nolto/impl.cpp'),
                    '-o',str(tmp/'sdk-relink')],check=True)
    subprocess.run([str(tmp/'sdk-relink'),addresses['safeGuard'],str(tmp/'symbols'),
                    hex(values['LCD_Refresh']),hex(values['MCS_GetVariable'])],check=True,env=environment)
