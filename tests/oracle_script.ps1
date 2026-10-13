param([string]$Dll, [string]$Script, [string]$Out, [string]$Log)
$src = @"
using System;
using System.IO;
using System.Globalization;
using System.Runtime.InteropServices;
public static class FlsS {
    [DllImport("kernel32", CharSet=CharSet.Ansi, SetLastError=true)] public static extern IntPtr LoadLibraryA(string p);
    [DllImport("kernel32", CharSet=CharSet.Ansi)] public static extern IntPtr GetProcAddress(IntPtr h, string n);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int DInit(ref int inst, uint sr);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int D1(int a);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int D2(int a, int b);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int D3(int a, int b, int c);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int D4(int a, int b, int c, int d);
    [UnmanagedFunctionPointer(CallingConvention.StdCall, CharSet=CharSet.Ansi)] public delegate int DLoad(int inst, int a, int b, string name, int c);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate IntPtr DName2(int a, int b);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate IntPtr DName4(int a, int b, int c, int d);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int DAlloc(int inst, int chan, int z, int bank, int patch, int key, int vel, IntPtr levels, int z2, IntPtr cb, int tag, int z3, int quality);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int DRender(int inst, int m1, IntPtr dest, int frames, int add, IntPtr rev, IntPtr chor, float revLevel, float chorLevel);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate void DCb(int a, int b, int tag);
    static T F<T>(IntPtr h, string n) { return (T)(object)Marshal.GetDelegateForFunctionPointer(GetProcAddress(h, n), typeof(T)); }
    static StreamWriter log;
    static int[] note = new int[512];
    static IntPtr[] lv = new IntPtr[512];
    static void Cb(int a, int b, int tag) { log.Write("end " + (tag - 1) + "\n"); if (tag - 1 < 512) note[tag - 1] = 0; }
    static float Fl(string s) { return float.Parse(s, CultureInfo.InvariantCulture); }
    static void SetLv(IntPtr p, string[] a, int i) {
        Marshal.WriteInt32(p, 0, int.Parse(a[i]));
        Marshal.Copy(BitConverter.GetBytes(Fl(a[i + 1])), 0, p + 4, 4);
        Marshal.WriteInt32(p, 8, int.Parse(a[i + 2]));
        Marshal.Copy(BitConverter.GetBytes(Fl(a[i + 3])), 0, p + 12, 4);
        Marshal.Copy(BitConverter.GetBytes(Fl(a[i + 4])), 0, p + 16, 4);
    }
    public static void Run(string dll, string script, string outp, string logp) {
        IntPtr h = LoadLibraryA(dll);
        if (h == IntPtr.Zero) throw new Exception("LoadLibrary failed");
        DCb cb = Cb;
        GCHandle.Alloc(cb);
        IntPtr cbp = Marshal.GetFunctionPointerForDelegate(cb);
        DRender render = F<DRender>(h, "RenderVoices");
        DAlloc alloc = F<DAlloc>(h, "AllocateNote");
        D4 freeNote = F<D4>(h, "FreeNote");
        int inst = -1, sr = 44100, cap = 0;
        float[] dest = null, rev = null, chor = null;
        GCHandle pd = new GCHandle(), pr = new GCHandle(), pc = new GCHandle();
        byte[] bytes = null;
        log = new StreamWriter(logp, false, new System.Text.UTF8Encoding(false));
        using (FileStream fs = new FileStream(outp, FileMode.Create))
        foreach (string raw in File.ReadAllLines(script)) {
            string line = raw.Trim();
            if (line.Length == 0) continue;
            int sp = line.IndexOf(' ');
            string cmd = sp < 0 ? line : line.Substring(0, sp);
            string arg = sp < 0 ? "" : line.Substring(sp + 1).Trim();
            string[] a = arg.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
            if (cmd == "sr") sr = int.Parse(arg);
            else if (cmd == "init") log.Write("init=" + F<DInit>(h, "InitSynthInstance")(ref inst, (uint)sr) + "\n");
            else if (cmd == "fx") { F<D2>(h, "SetUseReverb")(inst, int.Parse(a[0])); F<D2>(h, "SetUseChorus")(inst, int.Parse(a[1])); }
            else if (cmd == "atten") log.Write("atten=" + F<D2>(h, "SetAttenuation")(inst, int.Parse(arg)) + "\n");
            else if (cmd == "poly") log.Write("poly=" + F<D2>(h, "SetPolyphony")(inst, int.Parse(arg)) + "\n");
            else if (cmd == "load") { F<D2>(h, "LUClearSoundFont")(inst, 0); log.Write("load=" + F<DLoad>(h, "LULoadSoundFont")(inst, 0, 0, arg, 0) + "\n"); }
            else if (cmd == "names") {
                IntPtr bn = F<DName2>(h, "GetBankName")(inst, 0);
                log.Write("bank=" + (bn == IntPtr.Zero ? "" : Marshal.PtrToStringAnsi(bn)) + "\n");
                IntPtr pn = F<DName4>(h, "GetInstrPatchName")(inst, 0, int.Parse(a[0]), int.Parse(a[1]));
                log.Write("patch " + a[0] + " " + a[1] + "=" + (pn == IntPtr.Zero ? "" : Marshal.PtrToStringAnsi(pn)) + "\n");
            }
            else if (cmd == "on") {
                int slot = int.Parse(a[0]);
                if (lv[slot] == IntPtr.Zero) lv[slot] = Marshal.AllocHGlobal(20);
                SetLv(lv[slot], a, 6);
                note[slot] = alloc(inst, int.Parse(a[1]), 0, int.Parse(a[2]), int.Parse(a[3]), int.Parse(a[4]), int.Parse(a[5]), lv[slot], 0, cbp, slot + 1, 0, int.Parse(a[11]));
                log.Write("on " + slot + "=" + (note[slot] > 0 ? 1 : note[slot]) + "\n");
            }
            else if (cmd == "lv") { int slot = int.Parse(a[0]); if (lv[slot] != IntPtr.Zero) SetLv(lv[slot], a, 1); }
            else if (cmd == "off") { int slot = int.Parse(a[0]); if (note[slot] > 0) freeNote(inst, note[slot], 0, int.Parse(a[1])); }
            else if (cmd == "msg") F<D3>(h, "QueueShortMessage")(inst, (int)Convert.ToUInt32(arg, 16), 0);
            else if (cmd == "nrpn") F<D4>(h, "SetNRPN")(inst, int.Parse(a[0]), int.Parse(a[1]), int.Parse(a[2]));
            else if (cmd == "render") {
                int blocks = int.Parse(a[0]), frames = int.Parse(a[1]), sends = int.Parse(a[2]);
                float rl = Fl(a[3]), cl = Fl(a[4]);
                if (frames > cap) {
                    if (cap > 0) { pd.Free(); pr.Free(); pc.Free(); }
                    cap = frames;
                    dest = new float[cap * 2]; rev = new float[cap * 2]; chor = new float[cap * 2];
                    pd = GCHandle.Alloc(dest, GCHandleType.Pinned); pr = GCHandle.Alloc(rev, GCHandleType.Pinned); pc = GCHandle.Alloc(chor, GCHandleType.Pinned);
                    bytes = new byte[cap * 8];
                }
                for (int b = 0; b < blocks; b++) {
                    Array.Clear(dest, 0, frames * 2); Array.Clear(rev, 0, frames * 2); Array.Clear(chor, 0, frames * 2);
                    render(inst, -1, pd.AddrOfPinnedObject(), frames, 1, sends != 0 ? pr.AddrOfPinnedObject() : IntPtr.Zero, sends != 0 ? pc.AddrOfPinnedObject() : IntPtr.Zero, rl, cl);
                    Buffer.BlockCopy(dest, 0, bytes, 0, frames * 8); fs.Write(bytes, 0, frames * 8);
                    if (sends != 0) {
                        Buffer.BlockCopy(rev, 0, bytes, 0, frames * 8); fs.Write(bytes, 0, frames * 8);
                        Buffer.BlockCopy(chor, 0, bytes, 0, frames * 8); fs.Write(bytes, 0, frames * 8);
                    }
                }
            }
            else if (cmd == "dumpv") { byte[] db = new byte[0x8a * 0x248]; Marshal.Copy((IntPtr)Marshal.ReadInt32(h + 0xbb538), db, 0, db.Length); File.WriteAllBytes(arg + ".ref", db); }
            else if (cmd == "dump") { byte[] db = new byte[0xf5000]; Marshal.Copy(h + 0x1c000, db, 0, db.Length); File.WriteAllBytes(arg + ".ref", db); }
            else if (cmd == "free") F<D1>(h, "FreeSynthInstance")(inst);
        }
        log.Close();
    }
}
"@
Add-Type -TypeDefinition $src
[FlsS]::Run($Dll, $Script, $Out, $Log)
