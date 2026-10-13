import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUTPUT = os.path.join(HERE, 'gen', 'image.c')
USAGE = 'usage: make_image.py <path to the original FLSPro.dll>'

IMAGE_BASE = 0x10000000
LOADED_SECTIONS = ('.text', '.rdata', '.data', 'Shared')
BYTES_PER_LINE = 32

PE_OFFSET_FIELD = 0x3C
SECTION_COUNT_FIELD = 6
OPTIONAL_SIZE_FIELD = 20
OPTIONAL_HEADER = 24
SECTION_HEADER_SIZE = 40
SECTION_NAME_SIZE = 8


def read_sections(image):
    pe = struct.unpack_from('<I', image, PE_OFFSET_FIELD)[0]
    count = struct.unpack_from('<H', image, pe + SECTION_COUNT_FIELD)[0]
    optional_size = struct.unpack_from('<H', image, pe + OPTIONAL_SIZE_FIELD)[0]
    table = pe + OPTIONAL_HEADER + optional_size
    sections = []
    for index in range(count):
        header = table + index * SECTION_HEADER_SIZE
        name = image[header:header + SECTION_NAME_SIZE].rstrip(b'\0').decode()
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from('<IIII', image, header + SECTION_NAME_SIZE)
        sections.append((name, virtual_address, virtual_size, raw_offset, raw_size))
    return sections


def write_image(image, sections, output):
    table = []
    output.write('#include "../vm.h"\n\n')
    for name, virtual_address, virtual_size, raw_offset, raw_size in sections:
        if name not in LOADED_SECTIONS:
            continue
        data = image[raw_offset:raw_offset + min(raw_size, virtual_size)].rstrip(b'\0')
        symbol = 'sec_' + re.sub(r'\W', '', name)
        output.write('static const u8 %s[] = {\n' % symbol)
        for start in range(0, len(data), BYTES_PER_LINE):
            output.write(','.join('%d' % value for value in data[start:start + BYTES_PER_LINE]) + ',\n')
        output.write('0};\n\n')
        table.append((IMAGE_BASE + virtual_address, virtual_size, symbol, len(data)))
    output.write('const VmSection vm_sections[] = {\n')
    for address, virtual_size, symbol, size in table:
        output.write('    { 0x%xu, 0x%xu, %s, %du },\n' % (address, virtual_size, symbol, size))
    output.write('    { 0, 0, 0, 0 }\n};\n')


def main():
    if len(sys.argv) != 2 or not os.path.isfile(sys.argv[1]):
        print(USAGE)
        return 2
    with open(sys.argv[1], 'rb') as source:
        image = source.read()
    with open(OUTPUT, 'w') as output:
        write_image(image, read_sections(image), output)
    return 0


if __name__ == '__main__':
    sys.exit(main())
