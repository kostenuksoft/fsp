import array
import math
import sys

USAGE = 'usage: cmp.py <first.f32> <second.f32>'
SAMPLE_TYPE = 'f'
SILENCE_DB = -999.0
DB_PER_DECADE = 20


def load(path):
    samples = array.array(SAMPLE_TYPE)
    with open(path, 'rb') as source:
        samples.frombytes(source.read())
    return samples


def describe(first, second):
    count = min(len(first), len(second))
    peak = max((abs(value) for value in second), default=0.0) or 1.0
    differences = [abs(first[index] - second[index]) for index in range(count)]
    differing = sum(1 for value in differences if value > 0)
    largest = max(differences, default=0.0)
    first_index = next((index for index, value in enumerate(differences) if value > 0), -1)
    verdict = 'IDENTICAL' if differing == 0 and len(first) == len(second) else 'DIFFERENT'
    relative = DB_PER_DECADE * math.log10(largest / peak) if largest else SILENCE_DB
    return '%s: n=%d peak=%.6f maxdiff=%.3g (%.1f dB rel) differing=%d first=%d' % (
        verdict, count, peak, largest, relative, differing, first_index)


def main():
    if len(sys.argv) != 3:
        print(USAGE)
        return 2
    print(describe(load(sys.argv[1]), load(sys.argv[2])))
    return 0


if __name__ == '__main__':
    sys.exit(main())
