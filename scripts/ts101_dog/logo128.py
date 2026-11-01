"""128x32 boot animation: format 0xAB (see BootLogo.cpp)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image
from gsd32 import walking, sitting

AREA = 5 * 1024

def canvas(sprite, x, y=0):
    c = Image.new('L', (128, 32), 0)
    c.paste(sprite, (x, y), sprite)
    return c

def tobytes(img):  # 4 pages x 128 columns, LSB = top pixel
    px = img.load(); out = []
    for page in range(4):
        for x in range(128):
            b = 0
            for bit in range(8):
                if px[x, page * 8 + bit]: b |= 1 << bit
            out.append(b)
    return out

def runs(prev, cur):
    rs = []; i = 0
    while i < 512:
        if prev[i] == cur[i]: i += 1; continue
        j = i
        # extend the run, bridging gaps of up to 2 unchanged bytes (cheaper than a new 3 byte header)
        while j < 512 and (j // 128) == (i // 128) and j - i < 255:
            if prev[j] != cur[j]: j += 1; continue
            k = j
            while k < 512 and k - j < 3 and prev[k] == cur[k] and (k // 128) == (i // 128): k += 1
            if k < 512 and k - j < 3 and (k // 128) == (i // 128) and prev[k] != cur[k] and k - i < 255: j = k
            else: break
        rs.append((i, cur[i:j])); i = j
    return rs

def encode_frame(prev, cur):
    rs = runs(prev, cur)
    if not rs: return [0xFE]
    body = []
    for idx, data in rs: body += [idx >> 8, idx & 0xFF, len(data)] + list(data)
    if len(rs) > 0xFD or len(body) + 1 >= 513: return [0xFF] + cur
    return [len(rs)] + body

def encode(frames, delay):
    data = [0xAB, delay]; prev = [0] * 512
    for f in frames:
        cur = tobytes(f); data += encode_frame(prev, cur); prev = cur
    return data + [0x00]

def decode(data):  # mirrors the firmware decoder
    assert data[0] == 0xAB
    buf = [0] * 512; out = []; pos = 2
    while True:
        n = data[pos]
        if n == 0: break
        if n == 0xFE: pos += 1; out.append(list(buf)); continue
        if n == 0xFF: buf = list(data[pos + 1:pos + 513]); pos += 513
        else:
            pos += 1
            for _ in range(n):
                idx = (data[pos] << 8) | data[pos + 1]; ln = data[pos + 2]
                buf[idx:idx + ln] = data[pos + 3:pos + 3 + ln]; pos += 3 + ln
        out.append(list(buf))
        assert pos < AREA
    return out

def frames_ankommen():
    f = []
    xs = list(range(-66, 24, 5)) + [24]
    for i, x in enumerate(xs):
        f.append(canvas(walking(i % 6), x))
    f.append(canvas(walking(0), 24))
    sx = 34
    f.append(canvas(sitting('ground'), sx))
    for t in ['wag', 'ground'] * 3:
        f.append(canvas(sitting(t), sx))
    for k in range(3):
        f.append(canvas(sitting('ground', tongue=True), sx))
        f.append(canvas(sitting('ground'), sx))
    f.append(canvas(sitting('ground', tongue=True), sx))
    return f

LOGO_ADDR = 0x08000000 + 95 * 1024  # FLASH_LOGOADDR with TS101_DOG_UI (configuration.h)

def merge(firmware_hex, logo, out_hex):
    """Write firmware + logo as one hex file, so the stock bootloader flashes both in one go."""
    mem = {}; base = 0; start = None
    for line in open(firmware_hex, newline=''):
        line = line.strip()
        if not line.startswith(':'): continue
        raw = bytes.fromhex(line[1:]); assert sum(raw) & 0xFF == 0, line
        n, a, t, d = raw[0], (raw[1] << 8) | raw[2], raw[3], raw[4:-1]
        if t == 0:
            for i, b in enumerate(d): mem[base + a + i] = b
        elif t == 4: base = int.from_bytes(d, 'big') << 16
        elif t == 5: start = d
    assert max(mem) < LOGO_ADDR, 'firmware overlaps the logo area'
    for i, b in enumerate(logo + [0] * (AREA - len(logo))): mem[LOGO_ADDR + i] = b

    def rec(t, a, d):
        body = bytes([len(d), (a >> 8) & 0xFF, a & 0xFF, t]) + bytes(d)
        return ':' + (body + bytes([(-sum(body)) & 0xFF])).hex().upper() + '\r\n'
    out = []; hi = None; addrs = sorted(mem); i = 0
    while i < len(addrs):
        a = addrs[i]
        if a >> 16 != hi:
            hi = a >> 16; out.append(rec(4, 0, hi.to_bytes(2, 'big')))
        chunk = [mem[a]]; j = i + 1
        while j < len(addrs) and addrs[j] == a + len(chunk) and (addrs[j] & 0xF) and (addrs[j] >> 16) == hi:
            chunk.append(mem[addrs[j]]); j += 1
        out.append(rec(0, a & 0xFFFF, chunk)); i = j
    if start is not None: out.append(rec(5, 0, start))
    out.append(':00000001FF\r\n')
    open(out_hex, 'w', newline='').write(''.join(out))

if __name__ == '__main__':
    # usage: logo128.py <firmware.hex> <output.hex> [preview.gif]
    fr = frames_ankommen()
    data = encode(fr, 40)
    assert len(data) <= AREA, len(data)
    assert decode(data) == [tobytes(x) for x in fr]
    print(f'{len(fr)} frames, {len(data)} of {AREA} bytes')
    merge(sys.argv[1], data, sys.argv[2])
    print('wrote', sys.argv[2])
    if len(sys.argv) > 3:
        sc = 5
        big = [x.resize((128 * sc, 32 * sc), Image.NEAREST).convert('P') for x in fr]
        big[0].save(sys.argv[3], save_all=True, append_images=big[1:], duration=160, loop=0)
