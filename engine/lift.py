import json, struct, sys, os, re, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PCODE = os.path.join(ROOT, 're', 'out', 'FLSPro.dll.pcode.jsonl')
DLL = os.path.join(ROOT, 're', 'orig', 'FLSPro.dll')
GEN = os.path.join(HERE, 'gen')
IMAGE_BASE = 0x10000000
ENGINE_END = 0x10010d00

HOOKS = {
    0x10010eab: 'malloc',
    0x100115b7: 'malloc',
    0x10010fe5: 'free',
    0x100111ce: 'sprintf',
    0x1001172e: 'fclose',
}

EXPORTS = {}

def load():
    lines = open(PCODE).read().split('\n')
    regs = json.loads(lines[0])['regs']
    userops = json.loads(lines[1])['userops']
    funcs = [json.loads(x) for x in lines[2:] if x]
    return regs, userops, funcs

def pe_info():
    d = open(DLL, 'rb').read()
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    nsec = struct.unpack_from('<H', d, pe + 6)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + opt + i * 40
        name = d[o:o + 8].rstrip(b'\0').decode()
        vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
        secs.append((name, va, vs, ro, rs))
    def off(rva):
        for n, va, vs, ro, rs in secs:
            if va <= rva < va + max(vs, rs):
                return ro + rva - va
        raise KeyError(hex(rva))

    iat = {}
    irva = struct.unpack_from('<I', d, pe + 24 + 96 + 8)[0]
    o = off(irva)
    while True:
        oft, _, _, nrva, ft = struct.unpack_from('<IIIII', d, o)
        if not nrva:
            break
        dll = d[off(nrva):d.index(b'\0', off(nrva))].decode()
        t = off(oft or ft)
        k = 0
        while True:
            v = struct.unpack_from('<I', d, t + k * 4)[0]
            if not v:
                break
            nm = d[off(v) + 2:d.index(b'\0', off(v) + 2)].decode()
            iat[IMAGE_BASE + ft + k * 4] = nm
            k += 1
        o += 20

    erva = struct.unpack_from('<I', d, pe + 24 + 96)[0]
    eo = off(erva)
    nfun, nnames = struct.unpack_from('<II', d, eo + 20)
    afun, anames, aord = struct.unpack_from('<III', d, eo + 28)
    exports = {}
    for i in range(nnames):
        nm_rva = struct.unpack_from('<I', d, off(anames) + i * 4)[0]
        nm = d[off(nm_rva):d.index(b'\0', off(nm_rva))].decode()
        ordi = struct.unpack_from('<H', d, off(aord) + i * 2)[0]
        exports[nm] = IMAGE_BASE + struct.unpack_from('<I', d, off(afun) + ordi * 4)[0]
    return d, secs, iat, exports

GPR = {0: 'eax', 4: 'ecx', 8: 'edx', 0xc: 'ebx', 0x10: 'esp', 0x14: 'ebp', 0x18: 'esi', 0x1c: 'edi'}
FLAGN = {0x200: 'cf', 0x202: 'pf', 0x204: 'af', 0x206: 'zf', 0x207: 'sf', 0x20a: 'df', 0x20b: 'of'}
ITYPE = {1: 'u8', 2: 'u16', 4: 'u32', 8: 'u64'}
STYPE = {1: 'i8', 2: 'i16', 4: 'i32', 8: 'i64'}

class Cell:
    __slots__ = ('name', 'ctype', 'base', 'size', 'persist')
    def __init__(self, name, ctype, base, size, persist=True):
        self.name, self.ctype, self.base, self.size, self.persist = name, ctype, base, size, persist

CELLS = {}
def cell_for(off, size):

    if off < 0x20:
        base = off & ~3
        key = base
        if key not in CELLS:
            CELLS[key] = Cell(GPR[base], 'u32', base, 4)
    elif off == 0x110:
        key = off
        if key not in CELLS:
            CELLS[key] = Cell('fs', 'u32', off, 4)
    elif off in FLAGN and size == 1:
        key = off
        if key not in CELLS:
            CELLS[key] = Cell(FLAGN[off], 'u8', off, 1)
    elif off == 0x284:
        key = off
        if key not in CELLS:
            CELLS[key] = Cell('eip', 'u32', off, 4, persist=False)
    elif 0x1090 <= off <= 0x1093 and size == 1:
        key = off
        if key not in CELLS:
            CELLS[key] = Cell('c%d' % (off - 0x1090), 'u8', off, 1)
    elif off in (0x10a0, 0x10a1):
        key = 0x10a0
        if key not in CELLS:
            CELLS[key] = Cell('cw', 'u16', 0x10a0, 2)
    elif off in (0x10a2, 0x10a3):
        key = 0x10a2
        if key not in CELLS:
            CELLS[key] = Cell('sw', 'u16', 0x10a2, 2)
    elif off in (0x10a4, 0x10a6) and size == 2:
        key = off
        if key not in CELLS:
            CELLS[key] = Cell('fpu_%x' % off, 'u16', off, 2)
    elif off in (0x10a8, 0x10ac) and size == 4:
        key = off
        if key not in CELLS:
            CELLS[key] = Cell('fpu_%x' % off, 'u32', off, 4)
    elif 0x1100 <= off < 0x1180 and (off & 15) == 0 and size == 10:
        key = off
        if key not in CELLS:
            CELLS[key] = Cell('st%d' % ((off - 0x1100) >> 4), 'f80', off, 10)
    else:
        raise ValueError('unmapped register %x:%d' % (off, size))
    return CELLS[key]

ST = [0x1100 + 16 * i for i in range(8)]

class Lifter:
    def __init__(self, group, entries, iat):
        self.group = group
        self.entries = entries
        self.iat = iat
        self.used = {}
        self.written = set()
        self.uniq = {}
        self.labels = set()
        self.problems = []
        self.calls = set()
        self.out = []
        self.addrs = {i['a'] for f in group for i in f['insns']}

    def use(self, v, write=False):
        c = cell_for(v[1], v[2])
        self.used[c.name] = c
        if write:
            self.written.add(c.name)
        return c

    def rd(self, v):
        sp, off, size = v
        if sp == 'const':
            if size == 8:
                return 'UINT64_C(0x%x)' % (off & 0xffffffffffffffff)
            return '((%s)0x%xu)' % (ITYPE[size], off & ((1 << (8 * size)) - 1))
        if sp == 'register':
            c = self.use(v)
            if c.size == 10:
                return c.name
            if size == c.size:
                return c.name
            sh = (off - c.base) * 8
            return '((%s)(%s >> %d))' % (ITYPE[size], c.name, sh) if sh else '((%s)%s)' % (ITYPE[size], c.name)
        if sp == 'unique':
            return self.uq(v)
        if sp == 'ram':
            if size == 10:
                return 'vm_ld80(M, 0x%xu)' % off
            return 'RD%d(0x%xu)' % (size * 8, off)
        raise ValueError(sp)

    def uq(self, v):
        _, off, size = v
        name = 'u%x_%d' % (off, size)
        self.uniq[name] = 'f80' if size == 10 else ITYPE[size]
        return name

    def wr(self, v, expr):
        sp, off, size = v
        if sp == 'register':
            c = self.use(v, True)
            if c.size == 10 or size == c.size:
                return '%s = %s;' % (c.name, expr)
            sh = (off - c.base) * 8
            mask = ((1 << (8 * size)) - 1) << sh
            return '%s = (%s)((%s & ~(%s)0x%xu) | ((%s)(%s)(%s) << %d));' % (
                c.name, c.ctype, c.name, c.ctype, mask, c.ctype, ITYPE[size], expr, sh)
        if sp == 'unique':
            return '%s = %s;' % (self.uq(v), expr)
        if sp == 'ram':
            if size == 10:
                return 'vm_st80(M, 0x%xu, %s);' % (off, expr)
            return 'WR%d(0x%xu, %s);' % (size * 8, off, expr)
        raise ValueError(sp)

    def fl(self, v):
        sp, off, size = v
        if size == 10:
            return self.rd(v)
        if size == 8:
            return 'vm_f64(%s)' % self.rd(v)
        if size == 4:
            return 'vm_f32(%s)' % self.rd(v)
        raise ValueError('float size %d' % size)

    def wrf(self, v, expr):
        size = v[2]
        if size == 10:
            return self.wr(v, expr)
        if size == 8:
            return self.wr(v, 'vm_u64f(%s)' % expr)
        if size == 4:
            return self.wr(v, 'vm_u32f(%s)' % expr)
        raise ValueError('float size %d' % size)

    def goto(self, target, insn):

        if target in self.addrs:
            self.labels.add(target)
            return 'goto L_%x;' % target
        if target in self.entries:
            self.calls.add(target)
            return '{ @FLUSH@ f_%x(c); return; }' % target
        self.problems.append('branch to unknown %x at %x' % (target, insn['a']))
        return '{ @FLUSH@ vm_trap(c, 0x%xu); return; }' % target

    def st_all(self, write):
        for off in ST:
            self.use(('register', off, 10), write)

    def push_st(self):
        self.st_all(True)
        return 'st7 = st6; st6 = st5; st5 = st4; st4 = st3; st3 = st2; st2 = st1; st1 = st0;'

    def pop_st(self):
        self.st_all(True)
        return 'st0 = st1; st1 = st2; st2 = st3; st3 = st4; st4 = st5; st5 = st6; st6 = st7;'

    def clear_c2(self):
        self.use(('register', 0x10a2, 2), True)
        self.use(('register', 0x1092, 1), True)
        return 'sw &= (u16)~0x0400u; c2 = 0;'

    def override(self, insn):
        m = insn['s'].split()[0]
        st0 = ('register', 0x1100, 10)
        st1 = ('register', 0x1110, 10)
        if m == 'FYL2X':
            self.use(st0); self.use(st1, True)
            return ['st1 = vm_fyl2x(st0, st1);', self.pop_st()]
        if m == 'FYL2XP1':
            self.use(st0); self.use(st1, True)
            return ['st1 = vm_fyl2xp1(st0, st1);', self.pop_st()]
        if m == 'F2XM1':
            self.use(st0, True)
            return ['st0 = vm_f2xm1(st0);']
        if m == 'FSCALE':
            self.use(st0, True); self.use(st1)
            return ['st0 = vm_fscale(st0, st1);']
        if m == 'FSIN':
            self.use(st0, True)
            return ['st0 = vm_fsin(st0);', self.clear_c2()]
        if m == 'FCOS':
            self.use(st0, True)
            return ['st0 = vm_fcos(st0);', self.clear_c2()]
        if m == 'FPTAN':
            self.use(st0, True)
            l = ['st0 = vm_ftan(st0);', self.push_st(), 'st0 = 1.0L;', self.clear_c2()]
            return l
        if m == 'FPATAN':
            self.use(st0); self.use(st1, True)
            return ['st1 = vm_fpatan(st0, st1);', self.pop_st()]
        if m in ('FPREM1', 'FPREM'):
            self.use(st0, True); self.use(st1)
            return ['st0 = vm_%s(st0, st1);' % m.lower(), self.clear_c2()]
        if m in ('FLDPI', 'FLDLN2', 'FLDL2E', 'FLDL2T', 'FLDLG2'):
            l = [self.push_st()]
            l.append('st0 = vm_%s();' % m.lower())
            return l
        if m == 'FSQRT':
            self.use(st0, True)
            return ['st0 = vm_fsqrt(st0);']
        return None

    def op(self, insn, idx, o):
        mn, out, ins = o[0], o[1], o[2:]
        rd, wr, fl, wrf = self.rd, self.wr, self.fl, self.wrf
        T = ITYPE.get(out[2]) if out else None
        def binop(sym):
            return wr(out, '(%s)(%s %s %s)' % (T, rd(ins[0]), sym, rd(ins[1])))
        def cmp(sym, signed=False):
            a, b = rd(ins[0]), rd(ins[1])
            if signed:
                s = STYPE[ins[0][2]]
                a, b = '(%s)%s' % (s, a), '(%s)%s' % (s, b)
            return wr(out, '(u8)(%s %s %s)' % (a, sym, b))
        if mn == 'COPY':
            if out[2] == 10 or ins[0][2] == 10:
                assert out[2] == 10 and ins[0][2] == 10, insn['s']
            return wr(out, rd(ins[0]))
        if mn == 'LOAD':
            a = rd(ins[1])
            if out[2] == 10:
                return wr(out, 'vm_ld80(M, %s)' % a)
            return wr(out, 'RD%d(%s)' % (out[2] * 8, a))
        if mn == 'STORE':
            a = rd(ins[1])
            if ins[2][2] == 10:
                return 'vm_st80(M, %s, %s);' % (a, rd(ins[2]))
            return 'WR%d(%s, %s);' % (ins[2][2] * 8, a, rd(ins[2]))
        if mn == 'INT_ADD': return binop('+')
        if mn == 'INT_SUB': return binop('-')
        if mn == 'INT_AND': return binop('&')
        if mn == 'INT_OR': return binop('|')
        if mn == 'INT_XOR': return binop('^')
        if mn == 'INT_MULT': return binop('*')
        if mn in ('INT_LEFT', 'INT_RIGHT', 'INT_SRIGHT'):
            fn = {'INT_LEFT': 'shl', 'INT_RIGHT': 'shr', 'INT_SRIGHT': 'sar'}[mn]
            return wr(out, 'vm_%s%d(%s, %s)' % (fn, out[2] * 8, rd(ins[0]), rd(ins[1])))
        if mn in ('INT_DIV', 'INT_REM', 'INT_SDIV', 'INT_SREM'):
            return wr(out, 'vm_%s%d(%s, %s)' % (mn[4:].lower(), out[2] * 8, rd(ins[0]), rd(ins[1])))
        if mn == 'INT_EQUAL': return cmp('==')
        if mn == 'INT_NOTEQUAL': return cmp('!=')
        if mn == 'INT_LESS': return cmp('<')
        if mn == 'INT_LESSEQUAL': return cmp('<=')
        if mn == 'INT_SLESS': return cmp('<', True)
        if mn == 'INT_SLESSEQUAL': return cmp('<=', True)
        if mn == 'INT_CARRY':
            t = ITYPE[ins[0][2]]
            return wr(out, '(u8)((%s)(%s + %s) < %s)' % (t, rd(ins[0]), rd(ins[1]), rd(ins[0])))
        if mn in ('INT_SCARRY', 'INT_SBORROW'):
            fn = 'add' if mn == 'INT_SCARRY' else 'sub'
            return wr(out, 'vm_%s_ovf%d(%s, %s)' % (fn, ins[0][2] * 8, rd(ins[0]), rd(ins[1])))
        if mn == 'INT_ZEXT':
            return wr(out, '(%s)%s' % (T, rd(ins[0])))
        if mn == 'INT_SEXT':
            return wr(out, '(%s)(%s)(%s)%s' % (T, STYPE[out[2]], STYPE[ins[0][2]], rd(ins[0])))
        if mn == 'INT_2COMP':
            return wr(out, '(%s)(0 - %s)' % (T, rd(ins[0])))
        if mn == 'INT_NEGATE':
            return wr(out, '(%s)(~%s)' % (T, rd(ins[0])))
        if mn == 'SUBPIECE':
            assert ins[0][2] != 10
            return wr(out, '(%s)(%s >> %d)' % (T, rd(ins[0]), ins[1][1] * 8))
        if mn == 'POPCOUNT':
            return wr(out, '(%s)__builtin_popcountll((u64)%s)' % (T, rd(ins[0])))
        if mn == 'BOOL_NEGATE':
            return wr(out, '(u8)(!%s)' % rd(ins[0]))
        if mn == 'BOOL_AND': return wr(out, '(u8)((%s & %s) & 1)' % (rd(ins[0]), rd(ins[1])))
        if mn == 'BOOL_OR': return wr(out, '(u8)((%s | %s) & 1)' % (rd(ins[0]), rd(ins[1])))
        if mn == 'BOOL_XOR': return wr(out, '(u8)((%s ^ %s) & 1)' % (rd(ins[0]), rd(ins[1])))

        if mn == 'FLOAT_ADD': return wrf(out, '(%s + %s)' % (fl(ins[0]), fl(ins[1])))
        if mn == 'FLOAT_SUB': return wrf(out, '(%s - %s)' % (fl(ins[0]), fl(ins[1])))
        if mn == 'FLOAT_MULT': return wrf(out, '(%s * %s)' % (fl(ins[0]), fl(ins[1])))
        if mn == 'FLOAT_DIV': return wrf(out, '(%s / %s)' % (fl(ins[0]), fl(ins[1])))
        if mn == 'FLOAT_NEG': return wrf(out, '(-%s)' % fl(ins[0]))
        if mn == 'FLOAT_ABS': return wrf(out, '__builtin_fabsl(%s)' % fl(ins[0]))
        if mn == 'FLOAT_SQRT': return wrf(out, 'vm_fsqrt(%s)' % fl(ins[0]))
        if mn == 'FLOAT2FLOAT': return wrf(out, fl(ins[0]))
        if mn == 'INT2FLOAT':
            return wrf(out, '(f80)(%s)%s' % (STYPE[ins[0][2]], rd(ins[0])))
        if mn == 'TRUNC':
            return wr(out, 'vm_ftoi%d(%s)' % (out[2] * 8, fl(ins[0])))
        if mn == 'ROUND':
            return wrf(out, 'vm_frndint(%s)' % fl(ins[0]))
        if mn == 'FLOAT_NAN': return wr(out, '(u8)__builtin_isnan(%s)' % fl(ins[0]))
        if mn == 'FLOAT_LESS': return wr(out, '(u8)(%s < %s)' % (fl(ins[0]), fl(ins[1])))
        if mn == 'FLOAT_LESSEQUAL': return wr(out, '(u8)(%s <= %s)' % (fl(ins[0]), fl(ins[1])))
        if mn == 'FLOAT_EQUAL': return wr(out, '(u8)(%s == %s)' % (fl(ins[0]), fl(ins[1])))
        if mn == 'FLOAT_NOTEQUAL': return wr(out, '(u8)(%s != %s)' % (fl(ins[0]), fl(ins[1])))

        if mn in ('BRANCH', 'CBRANCH'):
            t = ins[0]
            if t[0] == 'const':
                rel = t[1] - (1 << (8 * t[2])) if t[1] >> (8 * t[2] - 1) else t[1]
                self.plabels.add(idx + rel)
                g = 'goto P_%x_%d;' % (insn['a'], idx + rel)
            else:
                g = self.goto(t[1], insn)
            if mn == 'BRANCH':
                return g
            return 'if (%s) %s' % (rd(ins[1]), g)
        if mn == 'BRANCHIND':
            t = rd(ins[0])
            if not insn['fl']:

                return '{ u32 t_ = %s; @FLUSH@ vm_call_ind(c, t_); return; }' % t
            l = ['switch (%s) {' % t]
            for x in sorted(set(insn['fl'])):
                l.append('case 0x%xu: %s' % (x, self.goto(x, insn)))
            l.append('default: { @FLUSH@ vm_trap(c, 0x%xu); return; } }' % insn['a'])
            return '\n    '.join(l)
        if mn == 'CALL':
            t = ins[0][1]
            self.calls.add(t)
            if t not in self.entries:
                self.problems.append('call to unknown %x at %x' % (t, insn['a']))
                return '{ @FLUSH@ vm_trap(c, 0x%xu); @RELOAD@ }' % t
            return '{ @FLUSH@ f_%x(c); @RELOAD@ }' % t
        if mn == 'CALLIND':
            return '{ u32 t_ = %s; @FLUSH@ vm_call_ind(c, t_); @RELOAD@ }' % rd(ins[0])
        if mn == 'RETURN':
            return '{ @FLUSH@ return; }'
        if mn == 'CALLOTHER':
            name = USEROPS[ins[0][1]]
            if name in ('LOCK', 'UNLOCK'):
                return ';'
            self.problems.append('userop %s at %x (%s)' % (name, insn['a'], insn['s']))
            return 'vm_trap(c, 0x%xu);' % insn['a']
        self.problems.append('unhandled op %s at %x' % (mn, insn['a']))
        return 'vm_trap(c, 0x%xu);' % insn['a']

    def lift(self):
        group = self.group
        f = group[0]
        body = []
        insns = sorted({i['a']: i for g in group for i in g['insns']}.values(), key=lambda i: i['a'])
        for g in group:
            self.labels.add(g['entry'])
        for k, insn in enumerate(insns):
            lines = []
            self.plabels = set()
            ov = self.override(insn)
            if ov is not None:
                lines = [(None, x) for x in ov]
            else:
                wrote_cw = False
                for idx, o in enumerate(insn['p']):
                    lines.append((idx, self.op(insn, idx, o)))
                    if o[1] and o[1][0] == 'register' and o[1][1] in (0x10a0, 0x10a1):
                        wrote_cw = True
                if wrote_cw:
                    self.st_all(True)
                    bar = ' '.join('VM_BARRIER(st%d);' % k for k in range(8))
                    lines.append((None, bar))
                    lines.append((None, 'vm_setcw(cw);'))
                    lines.append((None, bar))
                n = len(insn['p'])
                if n in self.plabels:
                    lines.append((n, ';'))

            last = insn['p'][-1][0] if insn['p'] else None
            nxt = insns[k + 1]['a'] if k + 1 < len(insns) else None
            if ov is None and last in ('BRANCH', 'RETURN', 'BRANCHIND'):
                pass
            elif insn['ft'] is None:
                if last not in ('BRANCH', 'RETURN', 'BRANCHIND'):
                    lines.append((None, '{ @FLUSH@ vm_trap(c, 0x%xu); return; }' % insn['a']))
            elif insn['ft'] != nxt:
                lines.append((None, self.goto(insn['ft'], insn)))
            body.append((insn, lines, set(self.plabels)))

        persist = [c for c in self.used.values() if c.persist]
        persist.sort(key=lambda c: c.base)
        flush = ' '.join('c->%s = %s;' % (c.name, c.name) for c in persist if c.name in self.written)
        reload_ = ' '.join('%s = c->%s;' % (c.name, c.name) for c in persist)
        out = []
        if len(group) == 1:
            out.append('void f_%x(CPU *c)\n{' % f['entry'])
        else:
            out.append('static void g_%x(CPU *c, i32 entry_)\n{' % f['entry'])
        out.append('    u8 *const M = vm_mem; (void)M;')
        for c in sorted(self.used.values(), key=lambda c: c.base):
            if c.persist:
                out.append('    %s %s = c->%s;' % (c.ctype, c.name, c.name))
            else:
                out.append('    %s %s = 0; (void)%s;' % (c.ctype, c.name, c.name))
        byt = collections.defaultdict(list)
        for n, t in sorted(self.uniq.items()):
            byt[t].append(n)
        for t, names in byt.items():
            for i in range(0, len(names), 12):
                out.append('    %s %s;' % (t, ', '.join(names[i:i + 12])))
        if len(group) > 1:
            out.append('    switch (entry_) {')
            for n, g in enumerate(group):
                out.append('    case %d: goto L_%x;' % (n, g['entry']))
            out.append('    default: return;')
            out.append('    }')
        for insn, lines, plabels in body:
            if insn['a'] in self.labels:
                out.append('L_%x:' % insn['a'])
            for idx, text in lines:
                if idx is not None and idx in plabels:
                    out.append('P_%x_%d:' % (insn['a'], idx))
                out.append('    ' + text.replace('@FLUSH@', flush).replace('@RELOAD@', reload_))
        out.append('}\n')
        if len(group) > 1:
            for n, g in enumerate(group):
                out.append('void f_%x(CPU *c) { g_%x(c, %d); }' % (g['entry'], f['entry'], n))
            out.append('')
        return '\n'.join(out)

def main():
    global USEROPS
    regs, USEROPS, funcs = load()
    d, secs, iat, exports = pe_info()
    byentry = {f['entry']: f for f in funcs}
    entries = set(byentry)

    roots = set(exports.values())

    for name, va, vs, ro, rs in secs:
        if name in ('.rdata', '.data', 'Shared'):
            blob = d[ro:ro + rs]
            for i in range(0, len(blob) - 3, 4):
                v = struct.unpack_from('<I', blob, i)[0]
                if v in entries and v < ENGINE_END:
                    roots.add(v)
    todo = list(roots)
    reach = set()
    while todo:
        a = todo.pop()
        if a in reach or a not in byentry:
            continue
        reach.add(a)
        if a in HOOKS:
            continue
        for i in byentry[a]['insns']:
            for o in i['p']:
                if o[0] in ('CALL', 'BRANCH', 'CBRANCH') and o[2][0] == 'ram' and o[2][1] in entries:
                    todo.append(o[2][1])
                for v in o[2:]:
                    if v and v[0] == 'const' and v[2] == 4 and v[1] in entries:
                        todo.append(v[1])
    os.makedirs(GEN, exist_ok=True)
    lifted = sorted(a for a in reach if a not in HOOKS)
    hooked = sorted(a for a in reach if a in HOOKS)
    problems = []
    texts = {}
    used_imports = collections.Counter()

    owner = {}
    for a in lifted:
        for i in byentry[a]['insns']:
            owner.setdefault(i['a'], a)
    parent = {a: a for a in lifted}
    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x
    for a in lifted:
        own = {i['a'] for i in byentry[a]['insns']}
        for i in byentry[a]['insns']:
            tg = [o[2][1] for o in i['p'] if o[0] in ('BRANCH', 'CBRANCH') and o[2][0] == 'ram']
            tg += [x for x in i['fl'] if any(o[0] == 'BRANCHIND' for o in i['p'])]
            if i['ft'] is not None:
                tg.append(i['ft'])
            for t in tg:
                if t not in own and t not in entries and t in owner:
                    parent[find(a)] = find(owner[t])
    groups = collections.defaultdict(list)
    for a in lifted:
        groups[find(a)].append(a)
    order = []
    for a in lifted:
        g = groups[find(a)]
        if g[0] != a:
            continue
        order.append(a)
        L = Lifter([byentry[x] for x in g], entries, iat)
        texts[a] = L.lift()
        problems += ['%s: %s' % (byentry[a]['func'], p) for p in L.problems]
    for a in lifted:
        for i in byentry[a]['insns']:
            for o in i['p']:
                for v in o[1:]:
                    if v and v[0] == 'ram' and v[1] in iat:
                        used_imports[iat[v[1]]] += 1

    cells = sorted((c for c in CELLS.values() if c.persist), key=lambda c: c.base)
    with open(os.path.join(GEN, 'vm_cells.h'), 'w') as h:
        h.write('#pragma once\n#include "../../common/fsp_types.h"\n')
        h.write('typedef struct CPU {\n')
        for c in cells:
            if c.ctype == 'f80':
                h.write('    f80 %s;\n' % c.name)
        for sz in (4, 2, 1):
            for c in cells:
                if c.ctype != 'f80' and c.size == sz:
                    h.write('    %s %s;\n' % (c.ctype, c.name))
        h.write('} CPU;\n\n')
        for a in lifted:
            h.write('void f_%x(CPU *c);\n' % a)
        for a in hooked:
            h.write('void f_%x(CPU *c);\n' % a)
        h.write('\n')
        for nm, a in sorted(exports.items()):
            h.write('#define FLS_%s f_%x\n' % (re.sub(r'\W', '_', nm), a))

    NPART = 6
    total = sum(len(texts[a]) for a in order)
    part, acc, parts = [], 0, []
    for a in order:
        part.append(a)
        acc += len(texts[a])
        if acc > total / NPART:
            parts.append(part); part, acc = [], 0
    if part:
        parts.append(part)
    for old in os.listdir(GEN):
        if old.startswith('lifted_'):
            os.remove(os.path.join(GEN, old))
    for n, part in enumerate(parts):
        with open(os.path.join(GEN, 'lifted_%d.c' % n), 'w') as c:
            c.write('#include "../vm.h"\n\n')
            for a in part:
                c.write(texts[a] + '\n')

    with open(os.path.join(GEN, 'dispatch.c'), 'w') as c:
        c.write('#include "../vm.h"\n\n')
        for a in hooked:
            c.write('void f_%x(CPU *c) { vm_hook_%s(c); }\n' % (a, HOOKS[a]))
        c.write('\nvoid vm_call_ind(CPU *c, u32 target)\n{\n    switch (target) {\n')
        for a in lifted + hooked:
            c.write('    case 0x%xu: f_%x(c); return;\n' % (a, a))
        c.write('    default: vm_import(c, target); return;\n    }\n}\n\n')

        c.write('const VmImport vm_imports[] = {\n')
        for slot, nm in sorted(iat.items()):
            c.write('    { 0x%xu, "%s" },\n' % (slot, nm))
        c.write('    { 0, 0 }\n};\n')

    print('lifted %d functions (%d hooked), %d instructions' % (
        len(lifted), len(hooked), sum(len(byentry[a]['insns']) for a in lifted)))
    crt = [a for a in lifted if a >= ENGINE_END]
    print('CRT functions lifted:', ' '.join('%s@%x' % (byentry[a]['func'], a) for a in crt))
    print('imports referenced:', ' '.join(sorted(used_imports)))
    print('%d problems' % len(problems))
    for p in problems:
        print('  ', p)

if __name__ == '__main__':
    main()
