"""Compile the portable BVH shadow shader offline (DXC + SPIRV-Cross)."""
import argparse
import pathlib
import re
import subprocess
import tempfile
from portable_shader_bindings import validate_metal_bindings
from portable_shader_source import source_digest

parser = argparse.ArgumentParser()
parser.add_argument('--dxc')
parser.add_argument('--spirv-cross')
parser.add_argument('--check', action='store_true')
parser.add_argument('--shader', choices=['warp_lookup_portable', 'warp_reflection_portable', 'ray_materials_portable', 'temporal_resample_portable', 'temporal_hud_portable', 'temporal_inputs_portable', 'background2_portable', 'background_portable', 'ray_geometry_portable', 'motion_portable', 'projected_text_portable', 'particle_portable', 'dust_portable', 'grid_spans_portable', 'grid_portable', 'warp_expand_portable', 'warp_material_portable', 'colour_warp_portable', 'axis_portable', 'shadow_portable', 'raster_portable', 'raster_bins', 'composite_portable', 'projection_portable', 'visibility_portable', 'transform_portable', 'continuous_portable', 'continuous_visibility_portable', 'clip_portable', 'clip_continuous_portable', 'spans_portable', 'bsp_portable', 'surface_portable', 'scene_portable', 'billboard_portable'], default='shadow_portable')
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
source = root / f'src/render/shaders/{args.shader}.hlsl'
destination = root / f'src/render/shaders/generated/{args.shader}.hpp'
stamp = '// Source SHA-256: ' + source_digest(source)
dxil_enabled = True
platform_payloads = args.shader in ('shadow_portable', 'surface_portable', 'billboard_portable',
                                   'clip_portable', 'clip_continuous_portable', 'spans_portable')
compiler_stamp = '// Strict IEEE (-Gis): enabled' if args.shader in ('temporal_inputs_portable', 'background2_portable', 'projected_text_portable', 'particle_portable', 'dust_portable', 'axis_portable', 'clip_continuous_portable', 'continuous_portable', 'billboard_portable') else ''
if args.check:
    validate_metal_bindings(destination.read_text())
    if stamp not in destination.read_text().splitlines()[:3]:
        raise SystemExit('Portable shadow shader is stale.')
    if dxil_enabled and 'unsigned char dxil[]=' not in destination.read_text():
        raise SystemExit('Portable shader is missing DXIL; regenerate.')
    if args.shader == 'clip_continuous_portable' and 'unsigned char intel_dxil[]=' not in destination.read_text():
        raise SystemExit('Continuous clipping shader is missing Intel DXIL; regenerate.')
    if platform_payloads and any(guard not in destination.read_text() for guard in ('#if defined(_WIN32)', '#if defined(__APPLE__)')):
        raise SystemExit('Portable shadow platform payload guards are stale.')
    if compiler_stamp and compiler_stamp not in destination.read_text().splitlines()[:4]:
        raise SystemExit('Portable clipping shader requires strict IEEE regeneration.')
    raise SystemExit(0)
if not args.dxc or not args.spirv_cross:
    parser.error('--dxc and --spirv-cross required')
with tempfile.TemporaryDirectory() as temporary:
    spv = pathlib.Path(temporary) / 'shadow.spv'
    dxil = pathlib.Path(temporary) / 'shadow.dxil'
    msl = pathlib.Path(temporary) / 'shadow.msl'
    strict = ['-Gis'] if compiler_stamp else []
    subprocess.run([args.dxc, *strict, '-spirv', '-fspv-target-env=vulkan1.0', '-T', 'cs_6_0', '-E', 'main',
                    '-Fo', str(spv), str(source)], check=True)
    subprocess.run([args.spirv_cross, str(spv), '--msl', '--msl-version', '20100', '--output', str(msl)], check=True)
    # SPIRV-Cross emits whitespace-only lines in its MSL template. Normalize
    # them before embedding so generated headers pass git's whitespace check.
    metal = '\n'.join(line.rstrip() for line in msl.read_text().splitlines()) + '\n'
    native_text = ''
    if dxil_enabled:
        # DXIL does not permit a non-inlined struct-return helper. Keep the
        # software binary64 arithmetic, but inline it for this backend only.
        subprocess.run([args.dxc, *strict, '-D', 'SF_NOINLINE=', '-T', 'cs_6_0', '-E', 'main',
                        '-Fo', str(dxil), str(source)], check=True)
        def native_array(name, blob):
            return '\ninline constexpr unsigned char ' + name + '[]={\n' + ',\n'.join(
                ','.join(str(b) for b in blob[i:i+32]) for i in range(0,len(blob),32)) + '\n};\n'
        native_text = native_array('dxil', dxil.read_bytes())
        if args.shader == 'clip_continuous_portable':
            intel_dxil = pathlib.Path(temporary) / 'clip_continuous_intel.dxil'
            subprocess.run([args.dxc, *strict, '-D', 'SF_NOINLINE=',
                            '-D', 'STARFOX_CLIP_CAPACITY=96', '-T', 'cs_6_0', '-E', 'main',
                            '-Fo', str(intel_dxil), str(source)], check=True)
            native_text += native_array('intel_dxil', intel_dxil.read_bytes())
    bindings = {'Settings': 0, 'nodes': 1, 'triangles': 2, 'outputMask': 3} if args.shader == 'shadow_portable' else {
        'Settings': 0, 'commands': 1, 'rows': 2, 'indices': 3, 'texels': 4,
        'back_pixels': 5, 'back_surfaces': 6, 'geometry_planes': 7, 'back_depth': 8,
        'pixels': 9, 'surfaces': 10, 'geometry_depth': 11}
    if args.shader == 'composite_portable':
        bindings = {'Settings': 0, 'cpuPixels': 1, 'nativePixels': 2, 'nativeSurfaces': 3, 'palette': 4, 'latePixels': 5, 'backgroundPixels': 6,
                    'nativeDepth': 7, 'nativeMotion': 8, 'pixels': 9, 'surfaces': 10, 'edgeColours': 11, 'geometryDepth': 12, 'motion': 13}
    if args.shader == 'background_portable':
        bindings = {'Settings': 0, 'memory': 1, 'pixels': 2}
    if args.shader == 'background2_portable':
        bindings = {'Settings': 0, 'memory': 1, 'pixels': 2, 'prepared': 3}
    if args.shader == 'raster_bins':
        bindings = {'Settings': 0, 'commands': 1, 'rows': 2, 'indices': 3}
    if args.shader == 'grid_portable':
        bindings = {'Settings': 0, 'points': 1}
    if args.shader == 'dust_portable':
        bindings = {'Settings': 0, 'sourcePoints': 1, 'colours': 2, 'points': 3}
    if args.shader == 'particle_portable':
        bindings = {'Settings': 0, 'particles': 1, 'projected': 2}
    if args.shader == 'projected_text_portable':
        bindings = {'Settings': 0, 'glyphs': 1, 'pixels': 2}
    if args.shader == 'grid_spans_portable':
        bindings = {'Settings': 0, 'points': 1, 'spans': 2}
    if args.shader == 'billboard_portable':
        bindings = {'Settings': 0, 'spans': 1}
    if args.shader == 'projection_portable':
        bindings = {'Settings': 0, 'points': 1, 'projected': 2}
    if args.shader == 'motion_portable':
        bindings = {'Settings': 0, 'currentPoints': 1, 'previousPoints': 2, 'motion': 3}
    if args.shader == 'temporal_inputs_portable':
        bindings = {'Settings': 0, 'cameraDepth': 1, 'sourceMotion': 2, 'groundCoverage': 3}
    if args.shader == 'temporal_hud_portable':
        bindings = {'Settings': 0, 'packedPixels': 1}
    if args.shader == 'temporal_resample_portable':
        bindings = {'Settings': 0}
    if args.shader == 'ray_geometry_portable':
        bindings = {'Settings': 0, 'points': 1, 'residuals': 2, 'triangles': 3, 'positions': 4}
    if args.shader == 'ray_materials_portable':
        bindings = {'Settings': 0, 'topology': 1, 'corners': 2, 'polygons': 3, 'materials': 4, 'faceLookup': 5, 'outputMaterials': 6}
    if args.shader == 'warp_lookup_portable':
        bindings = {'Settings': 0, 'polygons': 1, 'corners': 2, 'faceLookup': 3}
    if args.shader == 'axis_portable':
        bindings = {'Settings': 0, 'points': 1, 'indices': 2, 'residuals': 3, 'centres': 4, 'centreResiduals': 5}
    if args.shader == 'visibility_portable':
        bindings = {'Settings': 0, 'projected': 1, 'faces': 2, 'visible': 3}
    if args.shader == 'transform_portable':
        bindings = {'Settings': 0, 'vertices': 1, 'poses': 2, 'transformed': 3}
    if args.shader == 'continuous_portable':
        bindings = {'Settings': 0, 'vertices': 1, 'poses': 2, 'outputPoints': 3, 'outputResiduals': 4}
    if args.shader == 'continuous_visibility_portable':
        bindings = {'Settings': 0, 'points': 1, 'faces': 2, 'visible': 3}
    if args.shader in ('clip_portable', 'clip_continuous_portable'):
        bindings = {'Settings': 0, 'points': 1, 'corners': 2, 'polygons': 3, 'visibility': 4, 'clipped': 5}
        if args.shader == 'clip_continuous_portable':
            bindings['projectionParams'] = 5
            bindings['pointResiduals'] = 6
            bindings['clipped'] = 7
        else:
            bindings['cameraPoints'] = 5
            bindings['clipped'] = 6
    if args.shader == 'spans_portable':
        bindings = {'Settings': 0, 'clipped': 1, 'materials': 2, 'order': 3, 'orderResults': 4, 'commands': 5, 'masks': 6}
    if args.shader == 'bsp_portable':
        bindings = {'Settings': 0, 'nodes': 1, 'visibility': 2, 'faces': 3, 'trees': 4, 'ordered': 5, 'results': 6}
    if args.shader == 'warp_reflection_portable':
        bindings = {'Settings': 0, 'descriptors': 1, 'traversal': 2, 'ordered': 3, 'combinedDescriptors': 4, 'result': 5, 'combinedOrder': 6}
    if args.shader == 'colour_warp_portable':
        bindings = {'Settings': 0, 'ordered': 1, 'traversal': 2, 'polygons': 3, 'visibility': 4, 'descriptors': 5, 'result': 6}
    if args.shader == 'warp_material_portable':
        bindings = {'Settings': 0, 'descriptors': 1, 'ordered': 2, 'normals': 3, 'diffuse': 4, 'depthColours': 5, 'textureLookup': 6, 'materials': 7}
    if args.shader == 'warp_expand_portable':
        bindings = {'Settings': 0, 'ordered': 1, 'traversal': 2, 'sourcePolygons': 3, 'sourceCorners': 4, 'sourceMaterials': 5, 'decoded': 6, 'textures': 7, 'coordinates': 8, 'polygons': 9, 'corners': 10, 'materials': 11}
    if args.shader == 'surface_portable':
        bindings = {'Settings': 0, 'points': 1, 'corners': 2, 'polygons': 3, 'materials': 4, 'normals': 5, 'outputMaterials': 6, 'geometryPlanes': 7}
    if args.shader == 'scene_portable':
        bindings = {'Settings': 0, 'frontPixels': 1, 'frontSurfaces': 2, 'backPixels': 3, 'backSurfaces': 4,
                    'frontDepth': 5, 'backDepth': 6, 'frontMotion': 7, 'backMotion': 8,
                    'pixels': 9, 'surfaces': 10, 'geometryDepth': 11, 'motion': 12}
    for name, slot in bindings.items():
        metal, count = re.subn(r'(\b' + name + r'\s*\[\[buffer\()\d+(\)\]\])',
                              lambda match: match[1] + str(slot) + match[2], metal)
        if count != 1:
            raise RuntimeError(f'Unexpected Metal binding: {name}')
    validate_metal_bindings(metal, bindings)
    blob = spv.read_bytes()
    rows = [','.join(str(b) for b in blob[i:i+32]) for i in range(0, len(blob), 32)]
    namespace = 'starfox::render::shadows::portable_shader' if args.shader == 'shadow_portable' else 'starfox::render::raster_shader'
    if args.shader == 'composite_portable':
        namespace = 'starfox::render::composite_shader'
    if args.shader == 'background_portable':
        namespace = 'starfox::render::background_shader'
    if args.shader == 'background2_portable':
        namespace = 'starfox::render::background2_shader'
    if args.shader == 'raster_bins':
        namespace = 'starfox::render::raster_bins_shader'
    if args.shader == 'projection_portable':
        namespace = 'starfox::render::projection_shader'
    if args.shader == 'motion_portable':
        namespace = 'starfox::render::motion_shader'
    if args.shader == 'ray_geometry_portable':
        namespace = 'starfox::render::ray_geometry_shader'
    if args.shader == 'ray_materials_portable':
        namespace = 'starfox::render::ray_materials_shader'
    if args.shader == 'visibility_portable':
        namespace = 'starfox::render::visibility_shader'
    if args.shader == 'transform_portable':
        namespace = 'starfox::render::transform_shader'
    if args.shader == 'continuous_portable':
        namespace = 'starfox::render::continuous_shader'
    if args.shader == 'continuous_visibility_portable':
        namespace = 'starfox::render::continuous_visibility_shader'
    if args.shader == 'clip_portable':
        namespace = 'starfox::render::clip_shader'
    if args.shader == 'clip_continuous_portable':
        namespace = 'starfox::render::clip_continuous_shader'
    if args.shader == 'spans_portable':
        namespace = 'starfox::render::spans_shader'
    if args.shader == 'bsp_portable':
        namespace = 'starfox::render::bsp_shader'
    if args.shader == 'colour_warp_portable':
        namespace = 'starfox::render::colour_warp_shader'
    if args.shader == 'warp_reflection_portable':
        namespace = 'starfox::render::warp_reflection_shader'
    if args.shader == 'warp_lookup_portable':
        namespace = 'starfox::render::warp_lookup_shader'
    if args.shader == 'warp_material_portable':
        namespace = 'starfox::render::warp_material_shader'
    if args.shader == 'warp_expand_portable':
        namespace = 'starfox::render::warp_expand_shader'
    if args.shader == 'surface_portable':
        namespace = 'starfox::render::surface_shader'
    if args.shader == 'scene_portable':
        namespace = 'starfox::render::scene_shader'
    if args.shader == 'billboard_portable':
        namespace = 'starfox::render::billboard_shader'
    if args.shader == 'axis_portable':
        namespace = 'starfox::render::axis_shader'
    if args.shader == 'grid_portable':
        namespace = 'starfox::render::grid_shader'
    if args.shader == 'dust_portable':
        namespace = 'starfox::render::dust_shader'
    if args.shader == 'particle_portable':
        namespace = 'starfox::render::particle_shader'
    if args.shader == 'projected_text_portable':
        namespace = 'starfox::render::projected_text_shader'
    if args.shader == 'grid_spans_portable':
        namespace = 'starfox::render::grid_spans_shader'
    if args.shader == 'temporal_inputs_portable':
        namespace = 'starfox::render::temporal_inputs_shader'
    if args.shader == 'temporal_hud_portable':
        namespace = 'starfox::render::temporal_hud_shader'
    if args.shader == 'temporal_resample_portable':
        namespace = 'starfox::render::temporal_resample_shader'
    if platform_payloads:
        native_text = '\n#if defined(_WIN32)\n' + native_text + '#endif\n'
    metal_text = '\ninline constexpr char metal[]=R"SFXMETAL(\n' + metal + ')SFXMETAL";\n'
    if platform_payloads:
        metal_text = '\n#if defined(__APPLE__)\n' + metal_text + '#endif\n'
    destination.write_text('// Generated by tools/generate_portable_shadows.py; do not edit.\n' + stamp
        + ('\n' + compiler_stamp if compiler_stamp else '')
        + '\n#pragma once\nnamespace ' + namespace + ' {\n'
        + 'inline constexpr unsigned char spirv[]={\n' + ',\n'.join(rows)
        + '\n};' + native_text + metal_text + '}\n',
        encoding='utf-8', newline='\n')
