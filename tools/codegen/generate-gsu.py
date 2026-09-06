"""Regenerate the checked-in, resumable GSU adaptation from pinned Ares v148.

Development-only: normal builds use the generated files and need neither
Python nor Ares. Opcode and device expressions retain their source order;
clock-capable calls become C++ coroutine awaits. No OS scheduler is imported.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess

PIN = '0aafd85789215e84e1e43415c07d4c88461b7899'
parser = argparse.ArgumentParser()
parser.add_argument('ares', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
out = root / 'src/simulation/gsu'
if subprocess.check_output(['git', '-C', str(args.ares), 'rev-parse', 'HEAD'], text=True).strip() != PIN:
    raise SystemExit('GSU generator requires the pinned Ares revision')
if subprocess.check_output(['git', '-C', str(args.ares), 'status', '--porcelain'], text=True).strip():
    raise SystemExit('GSU generator requires unmodified reference sources')
paths = ['ares/component/processor/gsu/instruction.cpp',
         'ares/component/processor/gsu/instructions.cpp',
         'ares/sfc/coprocessor/superfx/core.cpp',
         'ares/sfc/coprocessor/superfx/memory.cpp',
         'ares/sfc/coprocessor/superfx/timing.cpp',
         'ares/sfc/coprocessor/superfx/io.cpp']
notice = '''// SPDX-License-Identifier: ISC
// Generated adaptation of Ares v148, 0aafd85789215e84e1e43415c07d4c88461b7899.
// Copyright (c) 2004-2025 ares team, Near et al.
// See LICENSE-ARES.txt for the complete permission notice.
// Regenerate with tools/codegen/generate-gsu.py; do not edit manually.

'''
functions = []
for relative in paths:
    source = (args.ares / relative).read_text()
    for match in re.finditer(r'(?:inline )?auto (?:GSU|SuperFX)::(\w+)\(([^)]*)\) -> (\w+) \{', source):
        start = match.end()
        depth, end = 1, start
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        body = source[start:end - 1]
        body = body.replace('cpu.synchronize(*this);', '')
        body = body.replace('synchronize(cpu);', '')
        body = body.replace('if(scheduler.synchronizing()) break;', '')
        body = body.replace('Thread::step(clocks);', 'clock_wait(clocks);')
        body = body.replace('Thread::', '')
        body = body.replace('cpu.irq(1);', 'irq_line = true;').replace('cpu.irq(0);', 'irq_line = false;')
        functions.append(dict(name=match[1], args=match[2], result=match[3], body=body, source=relative))
async_names = {'clock_wait'} | {f['name'] for f in functions if f['name'].startswith('instruction')}
while True:
    before = set(async_names)
    for f in functions:
        if any(re.search(r'(?<![.\w])' + re.escape(name) + r'\(', f['body']) for name in async_names):
            async_names.add(f['name'])
    if before == async_names:
        break

def await_calls(body):
    # Work right-to-left so nested calls retain their original argument order.
    matches = list(re.finditer(r'(?<![.\w])(' + '|'.join(sorted(async_names)) + r')\(', body))
    for match in reversed(matches):
        depth, end = 1, match.end()
        while depth:
            depth += (body[end] == '(') - (body[end] == ')')
            end += 1
        body = body[:match.start()] + '(co_await ' + body[match.start():end] + ')' + body[end:]
    return body

declarations, definitions = [], []
for f in functions:
    asynchronous = f['name'] in async_names
    result = 'Task<' + f['result'] + '>' if asynchronous else f['result']
    arguments = f['args']
    if f['name'] == 'read':
        arguments = arguments.replace('n8 data', 'n8 data = 0')
    declarations.append(f"    auto {f['name']}({arguments}) -> {result};")
    body = f['body'].replace('for(u32 n : range(16))', 'for([[maybe_unused]] u32 n : range(16))')
    if asynchronous:
        body = await_calls(body)
        body = re.sub(r'\breturn\b', 'co_return', body)
        if f['name'] == 'instruction':
            body = body.replace('co_return instruction##name', 'co_return co_await instruction##name')
        if f['result'] == 'void':
            body += '  co_return;\n'
    definitions.append(f"// {f['source']}\nauto Core::{f['name']}({f['args']}) -> {result} {{{body}}}\n")
(out / 'generated-declarations.inl').write_text(notice + '\n'.join(declarations) + '\n')
(out / 'generated-core.inl').write_text('\n'.join(line.rstrip() for line in (notice + '\n'.join(definitions)).splitlines()) + '\n')
registers = 'ares/component/processor/gsu/registers.hpp'
(out / 'generated-registers.inl').write_text(notice + (args.ares / registers).read_text())
manifest = {'source_pin': PIN, 'source_sha256': {p: hashlib.sha256((args.ares / p).read_bytes()).hexdigest() for p in paths + [registers]},
            'coroutine_functions': sorted(async_names - {'clock_wait'})}
(out / 'source-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
print(f'Generated {len(functions)} functions; {len(async_names)-1} suspendable')
