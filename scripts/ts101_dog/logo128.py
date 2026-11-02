"""128x32 boot animation: format 0xAB (see BootLogo.cpp)."""
import sys, os; sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image

AREA = 2 * 1024
ART = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'art')

def portrait(sprite, y, smoke=()):
    """Draw on an upright 32x128 canvas and turn it onto the 128x32 panel, the top towards the tip (column 0)."""
    c = Image.new('L', (32, 128), 0)
    c.paste(sprite, (0, y), sprite)
    for sx, sy in smoke:
        if 0 <= sy < 128 and not c.getpixel((sx, sy)):
            c.putpixel((sx, sy), 255)
    return c.rotate(90, expand=True)

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

# Where the dog stops on the upright canvas, and the cigarette tip relative to the sprite
REST_Y = 72
EMBER = (3, 21)

def smoke_puffs(step):
    """Puffs rising from the cigarette, drifting from side to side as they go; a new one every 6 steps."""
    out = []
    for start in range(step % 6, step + 1, 6):
        h = 2 + (step - start) * 3
        y = REST_Y + EMBER[1] - h
        x = EMBER[0] + (1 if (h // 8) % 2 else 0)
        out += [(x, y), (x, y - 1), (x + 1, y - 2)]
    return out

def frames_portrait():
    dog = Image.open(os.path.join(ART, 'portrait_gucci.png')).convert('L').point(lambda v: 255 if v > 127 else 0)
    f = [portrait(dog, y) for y in (128, 118, 108, 99, 91, 84, 79, 75, REST_Y)]  # walks in from the cable end, slowing down
    f += [portrait(dog, REST_Y, smoke_puffs(i)) for i in range(36)]              # stands there smoking
    return f

LOGO_ADDR = 0x08000000 + 98 * 1024  # FLASH_LOGOADDR with TS101_DOG_UI (configuration.h)

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
    fr = frames_portrait()
    data = encode(fr, 40)
    assert len(data) <= AREA, len(data)
    assert decode(data) == [tobytes(x) for x in fr]
    print(f'{len(fr)} frames, {len(data)} of {AREA} bytes')
    merge(sys.argv[1], data, sys.argv[2])
    print('wrote', sys.argv[2])
    if len(sys.argv) > 3:
        sc = 5
        big = [x.rotate(-90, expand=True).resize((32 * sc, 128 * sc), Image.NEAREST).convert('P') for x in fr]  # shown upright
        big[0].save(sys.argv[3], save_all=True, append_images=big[1:], duration=160, loop=0)
