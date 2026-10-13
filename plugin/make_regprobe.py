import os

HERE = os.path.dirname(os.path.abspath(__file__))
SOURCE = os.path.join(HERE, 'regprobe.ps1')
HEADER = os.path.join(HERE, 'fsp_regprobe.h')
DATA = os.path.join(HERE, 'fsp_regprobe.cpp')
BACKSLASH = chr(92)
QUOTE = chr(34)
NEWLINE = chr(10)
CARRIAGE_RETURN = chr(13)
PARAMETER_LINE = 'param('


def quoted(line):
    escaped = line.replace(BACKSLASH, BACKSLASH + BACKSLASH).replace(QUOTE, BACKSLASH + QUOTE)
    return '    ' + QUOTE + escaped + BACKSLASH + 'n' + QUOTE


def script_lines():
    with open(SOURCE) as source:
        lines = source.read().replace(CARRIAGE_RETURN, '').split(NEWLINE)
    if lines[0].startswith(PARAMETER_LINE):
        return lines[1:]
    return lines


def header_lines():
    return [
        '#ifndef FSP_REGPROBE_H',
        '#define FSP_REGPROBE_H',
        '',
        '#include "fsp_def.h"',
        '',
        'extern const c8 RegProbe_Script[];',
        'extern const u32 RegProbe_ScriptSize;',
        '',
        '#endif',
    ]


def data_lines():
    lines = ['#include "fsp_regprobe.h"', '', 'extern const c8 RegProbe_Script[] =']
    lines += [quoted(line) for line in script_lines()]
    lines[-1] += ';'
    lines += ['', 'extern const u32 RegProbe_ScriptSize = sizeof(RegProbe_Script);']
    return lines


def write(path, lines):
    with open(path, 'w', newline=NEWLINE) as output:
        output.write(NEWLINE.join(lines) + NEWLINE)


def main():
    write(HEADER, header_lines())
    write(DATA, data_lines())


if __name__ == '__main__':
    main()
