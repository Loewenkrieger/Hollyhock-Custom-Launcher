#!/usr/bin/env python3
"""Verify the actual SH image, target guard, linked RAM and native test programs."""
import argparse
import json
from pathlib import Path
import struct
parser = argparse.ArgumentParser()
parser.add_argument('--variant', choices=['hh2','hh3'], required=True)
parser.add_argument('--outdir', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
version, full, guard = ('2000','02.01.2000.0000',b'2017.0512.1515') if args.variant == 'hh2' else ('7002','02.01.7002.0001',b'2023.0209.1500')
elf = (args.outdir / ('run.' + version + '.elf')).read_bytes()
binary = (args.outdir / ('run.' + version + '.bin')).read_bytes()
assert elf[:8] == b'\x7fELF\x01\x02\x01\x00'
assert struct.unpack_from('>HHI',elf,16) == (2,42,1)
assert struct.unpack_from('>I',elf,24)[0] == 0x8cfd0000
assert 0 < len(binary) <= 0x20000
assert guard + b'\0' in binary
address = json.loads((root / ('meta/hollyhock-3/versions/v' + full + '/addresses.json')).read_text())
assert address['safeGuard'].encode() == guard
required = {'LCD_GetSize','LCD_GetVRAMAddress','LCD_Refresh','GetInput','MCS_GetVariable','MCS_SetVariable','MCS_CreateFolder','GUI_DisplayMessageBox_Internal'}
assert required <= {entry['FunctionName'] for entry in address['addresses']}
prep = (root / ('obj-' + args.variant) / ('addresses.' + version + '.prep')).read_bytes()
assert prep in binary and b'HHK_SYMBOL_TABLE=' in binary and b'HHK_SYMBOL_TABLE_LEN=' in binary
cursor = prep.index(0)+1
mapping = {}
while cursor < len(prep):
    end = prep.index(0,cursor)
    name = prep[cursor:end].decode()
    cursor = end+1
    mapping[name] = int.from_bytes(prep[cursor:cursor+4],'big')
    cursor += 4
assert cursor == len(prep)
for symbol in required:
    expected = next(int(item['startAddress'],16) for item in address['addresses'] if item['FunctionName'] == symbol)
    assert mapping[symbol] == expected
print('PASS target SH ELF, RAM boundary and embedded OS symbol table for HH3 app handoff')
shared_app = (root/'smoke/hh3/launcher-test.hh3').read_bytes()
assert shared_app[:6] == b'\x7fELF\x01\x02'
assert b'Launcher test: HH3' in shared_app
print('PASS native HH3 SDK test app is included for the selected launcher')
if args.variant == 'hh2':
    hhk = (root / 'smoke/hh2/launcher-test.hhk').read_bytes()
    raw = (root / 'smoke/hh2/launcher-test.bin').read_bytes()
    assert hhk[:6] == b'\x7fELF\x01\x02'
    assert struct.unpack_from('>I',hhk,24)[0] == 0x8cff0000
    assert struct.unpack_from('>I',raw,12)[0] == 0x8cff0000
    assert b'Launcher test: HH2' in hhk
    assert len(raw) <= 0x10000
    table = struct.unpack_from('>I',hhk,32)[0]
    count = struct.unpack_from('>H',hhk,48)[0]
    allocated = []
    for i in range(count):
        _,kind,flags,address,offset,size = struct.unpack_from('>6I',hhk,table+i*40)
        if flags & 2 and size:
            assert kind in (1,8) and 0x8cff0000 <= address < address+size <= 0x8d000000
            if kind == 1: assert hhk[offset:offset+size] == raw[address-0x8cff0000:address-0x8cff0000+size]
            allocated.append((address,size,flags))
    assert any(a == 0x8cff0000 and f & 4 for a,_,f in allocated)
    assert struct.unpack_from('>H',raw,0)[0] & 0xf000 == 0xa000
    assert struct.unpack_from('>H',raw,2)[0] == 0x0009
    print('PASS real HH2 test HHK and BIN agree on the native load/entry address')
