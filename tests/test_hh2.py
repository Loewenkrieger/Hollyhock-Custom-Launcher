#!/usr/bin/env python3
"""Native HH2 file validation and real memory-copy tests, without SH execution."""
from pathlib import Path
from sdk_path import sdk_include
import os
import random
import struct
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def fixture(*, address=0x8cff0000, entry=None, bss=0x8cff0020, metadata=b'Fixture\0'):
    names = b'\0.text\0.bss\0.hollyhock_name\0.shstrtab\0'
    code = bytes.fromhex('d101000b000900098003733e800fffff')
    sections = [(0,0,0,b'',0), (1,6,address,code,len(code)), (8,3,bss,b'',8),
                (1,0,0,metadata,len(metadata)), (3,0,0,names,len(names))]
    offsets = [0,1,7,12,28]
    out = bytearray(52)
    headers = []
    for i,(kind,flags,addr,data,size) in enumerate(sections):
        while len(out)%4: out.append(0)
        offset = len(out); out += data
        headers.append(struct.pack('>10I',offsets[i],kind,flags,addr,offset,size,0,0,4,0))
    table = len(out)
    out += b''.join(headers)
    out[:52] = struct.pack('>16sHHIIIIIHHHHHH', b'\x7fELF\x01\x02\x01'+bytes(9),
                          2,42,1,address if entry is None else entry,0,table,0,52,0,0,40,len(sections),4)
    return bytes(out)

environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
passed = 0
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    parser = tmp/'parser'
    loader = tmp/'loader'
    flags = ['g++','-std=c++23','-O1','-g','-Wall','-Wextra','-Werror','-Wno-attributes',
             '-fno-sanitize-recover=all','-I'+str(root/'src'),'-I'+str(sdk_include(root))]
    subprocess.run(flags + ['-fsanitize=address,undefined',str(root/'tests/hh2_probe.cpp'),
                            str(root/'src/loaders/hh2/image.cpp'),'-o',str(parser)],check=True)
    subprocess.run(flags + ['-fsanitize=undefined',str(root/'tests/hh2_load_probe.cpp'),
                            str(root/'src/loaders/hh2/image.cpp'),str(root/'src/loaders/hh2/loader.cpp'),
                            str(root/'src/LazyFile.cpp'),'-Wl,--wrap=fread','-o',str(loader)],check=True)
    image = tmp/'app.hhk'
    def run(data,kind='hhk'):
        image.write_bytes(data)
        return subprocess.run([str(parser),kind,str(image)],capture_output=True,text=True,env=environment)
    def check(condition,message):
        global passed
        assert condition,message
        passed += 1; print('PASS',message)
    valid = fixture()
    r=run(valid)
    check(r.returncode == 0 and 'Fixture' in r.stdout and '8003733e800fffff' in r.stdout,
          'native HHK metadata and all OS addresses are preserved without translation')
    check(run(fixture(metadata=b'\0')).returncode == 0,'HHK without metadata values remains usable')
    check(run(fixture(address=0x8c200000,bss=0x8c200020)).returncode == 0,
          'native app RAM below the launcher supports custom linker locations')
    invalid = [
        (valid[:40],'short ELF'),
        (b'BAD!'+valid[4:],'bad magic'),
        (fixture(address=0x8cfd0000),'launcher overlap'),
        (fixture(address=0x8cfcfffc),'section crossing into launcher'),
        (fixture(address=0x8cfffffc),'RAM end overflow'),
        (fixture(address=0x8c100000),'OS workspace'),
        (fixture(address=0x8cff0001),'misaligned section'),
        (fixture(bss=0x8cff0008),'section overlap'),
        (fixture(entry=0x8cff0020),'entry in BSS'),
        (fixture(entry=0x8cff0001),'misaligned entry'),
        (fixture(metadata=b'no terminator'),'metadata without NUL'),
        (valid[:-1],'truncated section headers')]
    for offset,value,fmt,label in [(5,1,'B','little endian'),(18,3,'H','wrong CPU'),
                                    (16,3,'H','dynamic ELF'),(32,0xfffffff0,'I','section offset overflow'),
                                    (48,0xffff,'H','excessive sections')]:
        bad=bytearray(valid);struct.pack_into('>'+fmt,bad,offset,value);invalid.append((bad,label))
    for data,label in invalid: check(run(data).returncode == 1,'rejects '+label)
    raw = bytes.fromhex('0009000b00090009000000008cff0000')
    check(run(raw,'bin').returncode == 0,'BIN header uses its big-endian load address')
    result = run(raw + b'Binary name\0\0Description\0Author\0' + b'1.0\0', 'bin')
    check(result.returncode == 0 and 'Binary name' in result.stdout, 'bounded native BIN header metadata is displayed')
    check(run(raw + b'Unterminated text', 'bin').returncode == 0, 'optional malformed BIN metadata falls back without blocking launch')
    check(run(bytes.fromhex('0009000b'),'bin').returncode == 0,'headerless BIN uses the native default address')
    for address in [0x8cfd0000,0x8cfeffff,0x8cfffffe,0x8c100000,0x8cff0001]:
        data=bytearray(raw);struct.pack_into('>I',data,12,address)
        check(run(data,'bin').returncode == 1,'rejects invalid BIN destination '+hex(address))
    check(run(b'','bin').returncode == 1 and run(b'x','bin').returncode == 1,'empty and one-byte BINs are rejected')
    # Mutate all structural ELF fields; errors must remain ordinary parser failures.
    rng=random.Random(2000)
    for _ in range(200):
        bad=bytearray(valid)
        offset=rng.choice([16,18,24,32,40,46,48,50,len(valid)-40+16,len(valid)-40+20])
        bad[offset]^=rng.randrange(1,256)
        result=run(bad)
        assert result.returncode in (0,1),result.stderr
    check(True,'200 malformed native HHK records remain bounded under ASan/UBSan')
    for kind,data in [('hhk',valid),('bin',raw),('invalid',fixture(bss=0x8cfd0000)),('metadata',valid + bytes(2*1024*1024))]:
        image.write_bytes(data)
        subprocess.run([str(loader),kind,str(image)],check=True,env=environment)
        passed += 1
    print(str(passed)+' HH2 loader checks passed (no calculator execution)')
