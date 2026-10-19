#!/usr/bin/env python3

# Check code and text style:
# - LF newlines
# - no trailing whitespace
# - newline at end of file
# - yyjson.h and yyjson.c: at most 80 characters per line, ASCII only

import os, re, sys


SKIP = '''
.git
.cache
build*
bench
xcode
test/data
doc/doxygen
doc/images
'''


SKIP_RE = re.compile('|'.join(
    '^' + re.escape(line.strip().strip('/')).replace(r'\*', '[^/]*') + '(/|$)'
    for line in SKIP.splitlines() if line.strip()))


def skipped(path):
    return SKIP_RE.search(path.replace('\\', '/'))


def check(path):
    data = open(path, 'rb').read()
    if b'\0' in data:
        return []
    out = []
    strict = os.path.basename(path) in ('yyjson.h', 'yyjson.c')
    if data and not data.endswith(b'\n'):
        out.append('%s: missing newline at end of file' % path)
    if b'\r' in data:
        out.append('%s: line ending is not LF' % path)
    start = 0
    n = 1
    for i, b in enumerate(data):
        if b != 0x0a:
            continue
        line = data[start:i]
        if line.endswith(b'\r'):
            line = line[:-1]
        if line.endswith(b' ') or line.endswith(b'\t'):
            out.append('%s:%d: trailing whitespace' % (path, n))
        if strict:
            if len(line) > 80:
                out.append('%s:%d: line too long (%d > 80 characters)' % (
                    path, n, len(line)))
            if any(c >= 128 for c in line):
                out.append('%s:%d: non-ASCII' % (path, n))
        start = i + 1
        n += 1
    return out


def main():
    problems = []
    for dirpath, dirnames, filenames in os.walk('.'):
        rel = '' if dirpath == '.' else os.path.relpath(dirpath, '.')
        dirnames[:] = [d for d in dirnames
                       if not skipped(os.path.join(rel, d) if rel else d)]
        if rel and skipped(rel):
            continue
        for name in filenames:
            path = os.path.join(dirpath, name)
            if os.path.isfile(path):
                problems.extend(check(path))
    for item in problems:
        print(item)
    print('%d problems' % len(problems) if problems else 'ok')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
