"""Exercise the real CMake VR shader gate in disposable, asset-free projects."""
import argparse
import pathlib
import re
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--cmake', default='cmake')
parser.add_argument('--generator', default='Ninja')
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
files = set()


def include_graph(path):
    path = path.resolve()
    path.relative_to(root)  # Never copy external paths into the fixture.
    if path in files:
        return
    files.add(path)
    for name in re.findall(rb'^\s*#\s*include\s*"([^"\r\n]+)"', path.read_bytes(), re.MULTILINE):
        include_graph(path.parent / name.decode('utf-8'))


scene = root / 'src/vr/shaders/scene.hlsl'
ray = root / 'src/vr/shaders/ray_expand.hlsl'
include_graph(scene)
include_graph(ray)
for relative in ('cmake/VRShaderChecks.cmake', 'tools/generate_vr_shaders.py',
                 'tools/generate_vr_ray_shader.py', 'tools/portable_shader_source.py',
                 'src/vr/shaders/scene_spirv.hpp', 'src/vr/shaders/ray_expand_spirv.hpp'):
    files.add(root / relative)

with tempfile.TemporaryDirectory(prefix='starfox-vr-shader-gate-') as temporary:
    fixture = pathlib.Path(temporary) / 'source'
    build = pathlib.Path(temporary) / 'build'
    for source in files:
        target = fixture / source.relative_to(root)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    (fixture / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.24)\n'
        'project(VRShaderFreshness NONE)\n'
        'include(cmake/VRShaderChecks.cmake)\n', encoding='utf-8')

    def run(command, expected=None):
        result = subprocess.run(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        if (expected is None and result.returncode != 0) or (
                expected is not None and (result.returncode == 0 or expected not in result.stdout)):
            raise RuntimeError(f'Unexpected shader gate result ({result.returncode}):\n{result.stdout}')

    run([args.cmake, '-S', str(fixture), '-B', str(build), '-G', args.generator])
    build_command = [args.cmake, '--build', str(build)]
    run(build_command)
    cases = [
        ('src/vr/shaders/scene.hlsl', 'VR shaders stale', False),
        ('include/starfox/render/ex_face_planet_regions.inc', 'VR shaders stale', False),
        ('src/vr/shaders/ray_expand.hlsl', 'VR ray expansion shader is stale', False),
        ('src/vr/shaders/scene_spirv.hpp', 'VR shaders stale', True),
    ]
    # Also prove that a transitive ray helper, not just the entry point, is watched.
    helpers = sorted(path for path in files if path.suffix == '.hlsli')
    if not helpers:
        raise RuntimeError('Expected ray shader include graph is empty')
    cases.append((str(helpers[0].relative_to(root)), 'VR ray expansion shader is stale', False))
    for relative, error, header in cases:
        path = fixture / relative
        original = path.read_bytes()
        changed = original.replace(b'// Source SHA-256:', b'// Stale SHA-256:', 1) if header else original + b'\n// stale fixture\n'
        path.write_bytes(changed)
        run(build_command, error)
        path.write_bytes(original)
        run(build_command)
        print(f'Incremental stale rejection and recovery: {relative}')
print('VR scene/ray CMake freshness guards passed; production sources untouched.')
