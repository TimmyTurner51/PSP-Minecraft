import struct, sys, os

sys.stdout.reconfigure(encoding='utf-8', errors='replace')


def read_sfo(data):
    if data[:4] != b'\x00PSF':
        return None
    _, ver, keyofs, dataofs, nentries = struct.unpack_from('<4sIIII', data, 0)
    out = []
    for i in range(nentries):
        base = 20 + i * 16
        # entry: u16 nameofs, u8 alignment, u8 type, u32 size, u32 max, u32 dataofs
        nameofs, align, dtype = struct.unpack_from('<HBB', data, base)
        dlen, dmax = struct.unpack_from('<II', data, base + 4)
        dofs = struct.unpack_from('<I', data, base + 12)[0]
        name = data[keyofs + nameofs:data.index(b'\x00', keyofs + nameofs)].decode()
        raw = data[dataofs + dofs:dataofs + dofs + dlen]
        if dlen == 4:
            val = "%d" % struct.unpack_from('<I', raw, 0)[0]
        else:
            val = raw.rstrip(b'\x00').decode(errors='replace')
        out.append((name, val))
    return out


for path in sys.argv[1:]:
    print("=" * 62)
    print(os.path.basename(path))
    blob = open(path, 'rb').read()
    print("  file size: %d bytes" % len(blob))
    magic, ver = struct.unpack_from('<4sI', blob, 0)
    if magic != b'\x00PBP':
        print("  NOT a PBP (magic %r)" % magic)
        continue

    offs = struct.unpack_from('<8I', blob, 8)
    names = ['PARAM.SFO', 'ICON0.PNG', 'ICON1.PMF', 'PIC0.PNG',
             'PIC1.PNG', 'SND0.AT3', 'DATA.PSP', 'DATA.PSAR']
    for n, o in zip(names, offs):
        print("    %-11s offset 0x%08X" % (n, o))

    sfo_start = offs[0]
    nxt = [o for o in offs[1:] if o > sfo_start]
    sfo_end = min(nxt) if nxt else len(blob)
    print("  SFO fields:")
    for k, v in (read_sfo(blob[sfo_start:sfo_end]) or []):
        print("    %-15s %s" % (k, v))

    data_start = offs[6]
    data_end = offs[7] if offs[7] > data_start else len(blob)
    prx = blob[data_start:data_end]
    print("  DATA.PSP: %d bytes, magic %r" % (len(prx), prx[:4]))
    if prx[:4] == b'\x7fELF':
        etype = struct.unpack_from('<H', prx, 16)[0]
        kind = {0: 'NONE', 1: 'REL', 2: 'EXEC', 3: 'DYN',
                0xffa0: 'PRX (PSP relocatable)'}.get(etype, 'unknown')
        print("  ELF type: 0x%04X = %s" % (etype, kind))
        if etype == 0xffa0:
            print("  -> relocatable PRX, the same shape the game ships as")
        elif etype == 2:
            print("  -> static EXEC, NOT the shape the game ships as")
