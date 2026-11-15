#!/usr/bin/env python3
import io

p = 'test/unit/GoldenDocs_test.cpp'
with io.open(p, encoding='utf-8', newline='') as f:
    s = f.read()

subs = [
    ('of("S095Dock.Dock", "S095_Owner")', 'of("Dock", "S095Dock")'),
    ('of("S095Grid.Row", "S095_Owner")', 'of("Row", "S095Grid")'),
    ('of("S095Grid.Column", "S095_Owner")', 'of("Column", "S095Grid")'),
]
for a, b in subs:
    c = s.count(a)
    s = s.replace(a, b)
    print('replaced %d: %s' % (c, a))

with io.open(p, 'w', encoding='utf-8', newline='') as f:
    f.write(s)
