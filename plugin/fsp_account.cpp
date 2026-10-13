#include "fsp_account.h"

#include <string.h>

#include "fsp_gdi.h"
#include "fsp_text.h"

const c8 Account_KeyName[] = "Software\\Image-Line\\Registrations";
const c8 Account_ValueName[] = "FL Studio user name";
const c8 Account_DefaultName[] = "flchan";
const u32 Account_MaxShown = 32;
const c8 Account_WordGap = ' ';

static bool Account_IsDigit(c8 Symbol)
{
    return Symbol >= '0' && Symbol <= '9';
}

static bool Account_IsUpper(c8 Symbol)
{
    return Symbol >= 'A' && Symbol <= 'Z';
}

static bool Account_IsLower(c8 Symbol)
{
    return Symbol >= 'a' && Symbol <= 'z';
}

static void Account_ReadStored(c8 *Dest, DWORD DestSize)
{
    TRegKey Key = { NULL };
    DWORD Size = DestSize;
    DWORD Type = 0;

    Dest[0] = 0;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, Account_KeyName, 0, KEY_READ, &Key.Handle) != ERROR_SUCCESS) {
        Key.Handle = NULL;
        return;
    }
    if (RegQueryValueExA(Key.Handle, Account_ValueName, NULL, &Type, (BYTE *)Dest, &Size) != ERROR_SUCCESS) {
        Dest[0] = 0;
    }
    if (Type != REG_SZ) {
        Dest[0] = 0;
    }
    Dest[DestSize - 1] = 0;
}

static void Account_DropTrailingDigits(c8 *Name)
{
    usize Length = strlen(Name);

    while (Length > 0 && Account_IsDigit(Name[Length - 1])) {
        --Length;
        Name[Length] = 0;
    }
}

static void Account_SplitWords(const c8 *Joined, c8 *Dest, usize DestSize)
{
    usize Length = 0;

    for (usize Index = 0; Joined[Index] && Length + 1 < DestSize; ++Index) {
        bool StartsWord = Index > 0 && Account_IsUpper(Joined[Index]) && Account_IsLower(Joined[Index - 1]);

        if (StartsWord && Length + 2 < DestSize) {
            Dest[Length] = Account_WordGap;
            ++Length;
        }
        Dest[Length] = Joined[Index];
        ++Length;
    }
    Dest[Length] = 0;
}

void Account_GetName(c8 *Dest, usize DestSize)
{
    c8 Stored[Account_NameSize];
    c8 Shown[2 * Account_NameSize];

    Account_ReadStored(Stored, sizeof(Stored));
    Account_DropTrailingDigits(Stored);
    Account_SplitWords(Stored, Shown, sizeof(Shown));
    Shown[Account_MaxShown] = 0;
    if (Text_IsEmpty(Shown)) {
        Text_Copy(Dest, DestSize, Account_DefaultName);
    } else {
        Text_Copy(Dest, DestSize, Shown);
    }
}
