param([string]$Dll)
$src = @"
using System;
using System.Runtime.InteropServices;
public static class RegProbe {
    [DllImport("kernel32", CharSet=CharSet.Unicode, SetLastError=true)] public static extern IntPtr LoadLibraryW(string p);
    [DllImport("kernel32")] public static extern IntPtr VirtualAlloc(IntPtr a, UIntPtr n, uint t, uint p);
    [DllImport("kernel32")] public static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32")] public static extern bool TerminateProcess(IntPtr h, uint code);
    [DllImport("kernel32")] public static extern uint SetErrorMode(uint m);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)] public delegate int Thunk();
    public static string Run(string dll) {
        SetErrorMode(0x8003);
        IntPtr h = LoadLibraryW(dll);
        if (h == IntPtr.Zero) return "load failed " + Marshal.GetLastWin32Error();
        IntPtr host = Marshal.AllocHGlobal(4096);
        for (int i = 0; i < 4096; i += 4) Marshal.WriteInt32(host, i, 0);
        Marshal.WriteInt32(host, 4, 11001001);
        int[] argc = new int[] { 4, 3, 2, 4, 1, 2, 4, 0, 0, 2, 2, 7, 7, 4, 2, 1, 0, 0, 0, 3, 1, 2, 2, 2, 7, 1, 2, 5, 0, 0, 3, 4, 1, 1, 0, 0, 3, 3, 3, 1, 1, 4, 9, 1, 1 };
        IntPtr vt = Marshal.AllocHGlobal(4 * 64);
        IntPtr stubs = VirtualAlloc(IntPtr.Zero, (UIntPtr)4096, 0x3000, 0x40);
        for (int i = 0; i < 64; i++) {
            int k = i < argc.Length ? argc[i] : 1;
            IntPtr sp = stubs + i * 16;
            int m = 0;
            if (i == 17 || i == 18) { Marshal.WriteByte(sp, m++, 0xD9); Marshal.WriteByte(sp, m++, 0xEE); }
            Marshal.WriteByte(sp, m++, 0x31); Marshal.WriteByte(sp, m++, 0xC0);
            Marshal.WriteByte(sp, m++, 0xC2); Marshal.WriteInt16(sp, m, (short)((k + 1) * 4));
            Marshal.WriteInt32(vt, i * 4, sp.ToInt32());
        }
        Marshal.WriteInt32(host, 0, vt.ToInt32());
        IntPtr flags = h + 0x161f3c;
        int before = Marshal.ReadInt32(flags);
        IntPtr code = VirtualAlloc(IntPtr.Zero, (UIntPtr)64, 0x3000, 0x40);
        byte[] b = new byte[32];
        int n = 0;
        b[n++] = 0xB8; BitConverter.GetBytes(host.ToInt32()).CopyTo(b, n); n += 4;
        b[n++] = 0xBA; BitConverter.GetBytes(1).CopyTo(b, n); n += 4;
        b[n++] = 0xB9; BitConverter.GetBytes(0x90a6).CopyTo(b, n); n += 4;
        b[n++] = 0x68; BitConverter.GetBytes(flags.ToInt32()).CopyTo(b, n); n += 4;
        b[n++] = 0xBB; BitConverter.GetBytes((h + 0x107b5c).ToInt32()).CopyTo(b, n); n += 4;
        b[n++] = 0xFF; b[n++] = 0xD3;
        b[n++] = 0x0F; b[n++] = 0xB6; b[n++] = 0xC0;
        b[n++] = 0xC3;
        Marshal.Copy(b, 0, code, n);
        Thunk t = (Thunk)Marshal.GetDelegateForFunctionPointer(code, typeof(Thunk));
        int r = t();
        int after = Marshal.ReadInt32(flags);
        return "base=" + h.ToString("x") + " result=" + r + " flags before=" + before.ToString("x") + " after=" + after.ToString("x") + " demo=" + (((after & 0x8000) != 0) ? "YES" : "NO");
    }
}
"@
Add-Type -TypeDefinition $src
$r = [RegProbe]::Run($Dll)
[Console]::Out.WriteLine($r)
[Console]::Out.Flush()
[RegProbe]::TerminateProcess([RegProbe]::GetCurrentProcess(), 0) | Out-Null
