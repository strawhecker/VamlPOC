#!/usr/bin/env python3
"""End-of-11a feasibility scan: which of the 301 files can the lib parse at the
end of Sprint 11a? 11a lands S451-S456 (document/ns/comment grammar, property-element,
ResourceDictionary + x:Key + MergedDictionaries + Static/DynamicResource, Style/Setter/
triggers). The REGISTERED ELEMENT SET is still the current 12-element registry
(Window/StackPanel/Grid/WrapPanel/Canvas/ScrollViewer/TextBox/Button/CheckBox/Slider/Icon).
So 'can view at end of 11a' == uses ONLY that element surface + no foreign ns + no
unsupported core element."""
import os, re, collections

ROOT = r'D:\CardiMapper2\Cardi3\CardiMapper2\CardiMapper3'

# Core elements the CURRENT lib registers (S451-S456 does NOT add controls).
SUPPORTED = {
    'Window', 'StackPanel', 'Grid', 'WrapPanel', 'Canvas', 'ScrollViewer',
    'TextBox', 'Button', 'CheckBox', 'Slider', 'Icon',
    # authoring-grammar element/attr forms landed in 11a:
    'Style', 'Setter', 'ResourceDictionary',
    'ResourceDictionary.MergedDictionaries',
    # property-element containers (content into named DP) - the container ELEMENT
    # is the control, e.g. <Button.Content>; base control must be supported
}
# property-element suffixes are attached to a base control element
PROP_ELEM = '.Content', '.Resources', '.DataContext', '.Background', '.Triggers', '.Template'

def is_prop_elem(name):
    for s in PROP_ELEM:
        if name.endswith(s) and name != s[1:]:
            return name[: -len(s)]
    return None

files = []
for dp, _, fns in os.walk(ROOT):
    for fn in fns:
        if fn.lower().endswith('.xaml'):
            files.append(os.path.join(dp, fn))
files = sorted(files)

tag = re.compile(r'<(?P<close>/?)\s*(?P<name>[A-Za-z_][\w.:]*)')
results = []
for f in files:
    t = open(f, encoding='utf-8-sig', errors='replace').read()
    names = set()
    for m in tag.finditer(t):
        if m.group('close'):
            continue
        n = m.group('name')
        if n in ('?xml', '!', '--'):
            continue
        # normalize the closing of property elements to their base
        names.add(n)
    foreign = sorted(n for n in names if ':' in n)
    unsupported = set()
    for n in names:
        if ':' in n:
            continue
        if n in SUPPORTED:
            continue
        base = is_prop_elem(n)
        if base is not None and base in SUPPORTED:
            continue
        unsupported.add(n)
    results.append((f, names, foreign, unsupported))

ok = [r for r in results if not r[2] and not r[3]]
lines = []
lines.append('END-OF-11a FEASIBILITY SCAN')
lines.append('='*70)
lines.append('total files                  : %d' % len(files))
lines.append('files using ONLY current 11a surface + no foreign ns : %d' % len(ok))
lines.append('files NOT loadable yet       : %d' % (len(files)-len(ok)))
lines.append('')
lines.append('REGISTERED elements at end of 11a:')
lines.append('   ' + ', '.join(sorted(SUPPORTED)))
lines.append('')
for f, names, foreign, uns in ok:
    lines.append('  OK: %s' % os.path.relpath(f, ROOT))
lines.append('')
lines.append('UNSUPPORTED core element names used across all 301 (ranked) -> what 11b+ must add:')
c = collections.Counter()
for f, names, foreign, uns in results:
    for u in uns:
        c[u] += 1
for n, k in c.most_common():
    lines.append('   %5d  %s' % (k, n))
lines.append('')
lines.append('FOREIGN (3rd-party / design-time) namespace elements used (ranked):')
fc = collections.Counter()
for f, names, foreign, uns in results:
    for n in foreign:
        fc[n] += 1
for n, k in fc.most_common():
    lines.append('   %5d  %s' % (k, n))

out = r'D:\CardiMapper2\Cardi3\CardiMapper2\CardiMapper3\end_of_11a_feasibility.txt'
open(out, 'w', encoding='utf-8').write('\n'.join(lines) + '\n')
print('wrote', out)
print('total files:', len(files))
print('loadable at end of 11a:', len(ok))
print('not loadable:', len(files) - len(ok))
print()
for n, k in c.most_common(40):
    print('   %5d  %s' % (k, n))
