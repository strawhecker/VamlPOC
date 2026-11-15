#!/usr/bin/env python3
import io, re

p = 'test/unit/GoldenDocs_test.cpp'
with io.open(p, encoding='utf-8', newline='') as f:
    s = f.read()

props = ['S095Width', 'S095Height', 'S095Title', 'S095Content', 'S095Text',
         'S095IsEnabled', 'S095Margin', 'S095Fill']
total = 0
for pr in props:
    # Match a BARE attribute name (not preceded by an owner dot) in the value
    # form `Prop="..."` where the name immediately follows a `<`/space/`=` and is
    # a standalone x:name attr (e.g. `Button S095Width="` or `Button S095Width=\"`).
    # Guard: only replace when NOT already owner-dotted.
    pat = re.compile(r'(?<![.\w])' + re.escape(pr) + r'=(?:\"|\\")')
    def repl(m):
        return 'S095_Owner.' + pr + m.group(0)[len(pr):]
    s2, n = pat.subn(repl, s)
    total += n
    s = s2
    if n:
        print('converted %d bare %s' % (n, pr))

with io.open(p, 'w', encoding='utf-8', newline='') as f:
    f.write(s)
print('total', total)
