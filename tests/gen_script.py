import sys
import random
import struct


def presets(path):
    d = open(path, 'rb').read()
    i = d.find(b'phdr')
    if i < 0:
        return [(0, 0)]
    n = struct.unpack_from('<I', d, i + 4)[0]
    res = []
    for k in range(n // 38 - 1):
        o = i + 8 + k * 38
        preset, bank = struct.unpack_from('<HH', d, o + 20)
        res.append((preset, bank))
    return res or [(0, 0)]


def main():
    seed = int(sys.argv[1])
    sf = sys.argv[2]
    out = sys.argv[3]
    rng = random.Random(seed)
    sr = rng.choice([44100, 44100, 48000, 22050, 96000])
    lines = ['sr %d' % sr, 'init', 'atten %d' % rng.choice([0, 0, 0, 6, 20]),
             'fx %d %d' % (rng.randint(0, 1), rng.randint(0, 1)), 'poly %d' % rng.choice([128, 128, 32, 8]),
             'load ' + sf, 'names %d %d']
    sends = rng.randint(0, 1)
    frames = rng.choice([64, 256, 512, 441, 1024, 2048])
    patch, bank = rng.choice(presets(sf))
    if rng.random() < 0.05:
        patch = (patch + 1) % 128
    chan = 9 if bank == 128 else 0
    lines[-1] = lines[-1] % (patch, bank)
    quality = rng.choice([0, 0, 0, 0, 0, 16, 128, 256])
    live = {}
    nxt = 0

    def lv():
        return '%d %s %d %s %s' % (rng.randint(-64, 64), rng.choice(['1.0', '0.78125', '0.5', '0.25', '1.5']),
                                   rng.choice([0, 0, 0, 25, -50, 100, 1200, -700]),
                                   rng.choice(['0.0', '0.0', '0.5', '1.0']), rng.choice(['0.0', '0.0', '0.3']))

    for step in range(rng.randint(40, 120)):
        r = rng.random()
        if r < 0.35 and len(live) < 24:
            slot = nxt % 512
            nxt += 1
            key = rng.randint(21, 108) if rng.random() < 0.9 else rng.randint(0, 127)
            lines.append('on %d %d %d %d %d %d %s %d' % (slot, chan, bank, patch, key, rng.randint(1, 127), lv(),
                                                         quality))
            live[slot] = 1
        elif r < 0.55 and live:
            slot = rng.choice(list(live))
            lines.append('off %d %d' % (slot, 1 if rng.random() < 0.15 else 0))
            del live[slot]
        elif r < 0.65 and live:
            lines.append('lv %d %s' % (rng.choice(list(live)), lv()))
        elif r < 0.72:
            ranges = {11: (0, 5940), 13: (0, 5940), 14: (0, 127), 15: (0, 5940), 2: (0, 5900), 18: (-127, 127),
                      3: (0, 127), 21: (0, 127)}
            n = rng.choice(sorted(ranges))
            lines.append('nrpn %d %d %d' % (chan, n, rng.choice([0x8000, rng.randint(*ranges[n])])))
        elif r < 0.78:
            lines.append('msg %x' % (0xB0 + chan + (1 << 8) + (rng.randint(0, 127) << 16)))
        lines.append('render %d %d %d %s %s' % (rng.randint(1, 12), frames, sends, rng.choice(['1.0', '0.5', '2.0']),
                                                 rng.choice(['1.0', '0.25'])))
    for slot in list(live):
        lines.append('off %d 0' % slot)
    lines.append('render %d %d %d 1.0 1.0' % (rng.randint(20, 80), frames, sends))
    lines.append('free')
    open(out, 'w', newline=chr(10)).write(chr(10).join(lines) + chr(10))


if __name__ == '__main__':
    main()
