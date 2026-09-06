"""Link the new roll replay against the previous input bridge in isolation."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
build = root / 'build/current'
output = root / 'tmp/short-roll-baseline'
output.mkdir(parents=True, exist_ok=True)
source = output / 'game_simulation.cpp'
source.write_bytes(subprocess.check_output([
    'git', '-C', str(root), 'show',
    'a85dceb:src/simulation/game_simulation.cpp']))
compiler = 'C:/Strawberry/c/bin/g++.exe'
obj = output / 'game_simulation.obj'
subprocess.run([compiler, '-std=gnu++20', '-O3', '-DNDEBUG',
    '-D__FUNCSIG__=__PRETTY_FUNCTION__', '-I' + str(root / 'include'),
    '-c', str(source), '-o', str(obj)], check=True)
exe = output / 'short-roll-baseline.exe'
subprocess.run([compiler, '-O3',
    str(build / 'CMakeFiles/starfox_timing_parity_tests.dir/tests/timing_parity_tests.cpp.obj'),
    str(obj), *(str(build / library) for library in (
        'libstarfox_core.a', 'libretro_cpu_65816.a', 'libretro_cpu_core.a', 'libsnes_spc_core.a')),
    '-o', str(exe)], check=True)
lines = []
for game, rom, symbols in (
    ('Original', 'upstream-ultrastarfox/SF.SFC', 'upstream-ultrastarfox/SYMBOLS.TXT'),
    ('EX', 'tmp/runtime-inputs/starfox-ex/SFES.SFC', 'assets/symbols/starfox-ex.txt')):
    result = subprocess.run([str(exe), str(root / rom), str(root / symbols),
        '--short-taps-only'], capture_output=True, text=True)
    message = result.stderr.strip()
    assert result.returncode == 1 and 'presses=2' in message and 'rolled=0' in message, (
        game, result.returncode, result.stdout, result.stderr)
    line = f'{game} baseline: {message}'
    lines.append(line)
    print(line)
(output / 'result.txt').write_text('\n'.join(lines) + '\n')
