from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
header = root / 'include/starfox/simulation/snes_interrupts.hpp'
original = header.read_text()
folder = root / 'tmp/timer-mutations'
target = folder / 'starfox/simulation/snes_interrupts.hpp'
target.parent.mkdir(parents=True, exist_ok=True)
mutations = {
    'ack-during-hold': ('if (!irq_hold_) irq_latch_ = irq_pending_ = false;',
                        'irq_latch_ = irq_pending_ = false;'),
    'htime-off-by-one': ('(horizontal_ + 1U) * 4U', 'horizontal_ * 4U'),
    'ignore-poll-lock': ('if (locked_) return {};', 'if (false) return {};'),
}
lines = []
for name, (before, after) in mutations.items():
    assert original.count(before) == 1
    target.write_text(original.replace(before, after))
    exe = folder / (name + '.exe')
    subprocess.run(['C:/Strawberry/c/bin/g++.exe', '-std=c++20', '-O2',
        '-I' + str(folder), '-I' + str(root / 'tmp/ares-timing'),
        '-I' + str(root / 'tmp/ares-timing/nall'),
        str(root / 'tools/reference/timer_audit.cpp'), '-o', str(exe)], check=True)
    result = subprocess.run([str(exe)], capture_output=True, text=True)
    line = f'{name}: exit={result.returncode}; {result.stdout.strip()} {result.stderr.strip()}'.strip()
    print(line)
    lines.append(line)
    assert result.returncode == 1, 'Mutation was not detected'
(root / 'tmp/timer-mutation-results.txt').write_text('\n'.join(lines) + '\n')
