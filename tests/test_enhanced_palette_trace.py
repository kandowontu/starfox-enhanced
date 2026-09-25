import contextlib
import importlib.util
import io
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('palette_trace',
    Path(__file__).resolve().parents[1] / 'tools/check_enhanced_palette_trace.py')
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class PaletteTraceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'trace.log'

    def run_trace(self, sky, surface, frames=None, gap=None, **thresholds):
        frames = range(len(sky)) if frames is None else frames
        self.path.write_text('\n'.join(
            f'enhanced-backdrop-palette: frame={f} bg=465 sky=0,0,0,{s} surface=0,0,0,{p}'
            for f, s, p in zip(frames, sky, surface)))
        with contextlib.redirect_stdout(io.StringIO()):
            MODULE.check(self.path, 465, maximum_step_gap=gap,
                         **({'minimum_phases': 2, 'minimum_surface_phases': 2} | thresholds))

    def test_independent_surface_phases_pass(self):
        self.run_trace([1, 1, .8, .6, .6, .6], [1, 1, 1, .7, .7, .7])

    def test_frozen_surface_cannot_pass_with_changing_sky(self):
        with self.assertRaisesRegex(ValueError, 'surface phases'):
            self.run_trace([1, .8, .6], [1, 1, 1])

    def test_frozen_sky_rejected(self):
        with self.assertRaisesRegex(ValueError, 'sky phases'):
            self.run_trace([1, 1, 1], [1, .8, .6])

    def test_missing_presentation_rejected(self):
        with self.assertRaisesRegex(ValueError, 'missing presentations'):
            self.run_trace([1, .8, .6], [1, .8, .6], frames=[0, 1, 3])

    def test_nonfinite_surface_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Invalid photographic'):
            self.run_trace([1, .8, .6], [1, float('nan'), .6])

    def test_one_contiguous_fade_with_stable_ends(self):
        values = [1] * 3 + [.8] * 3 + [.6] * 5
        self.run_trace(values, values, gap=3)

    def test_multiple_fades_rejected_by_single_fade_contract(self):
        values = [1] * 3 + [.8] * 10 + [.6] * 5
        with self.assertRaisesRegex(ValueError, 'one source-cadence'):
            self.run_trace(values, values, gap=3)

    def test_invalid_phase_threshold_rejected(self):
        with self.assertRaisesRegex(ValueError, 'positive'):
            self.run_trace([1, .8, .6], [1, .8, .6], minimum_surface_phases=0)

    def test_surface_fade_outside_sky_transition_rejected(self):
        sky = [1] * 3 + [.8] * 3 + [.6] * 5
        surface = [1] * 2 + [.8] * 9
        with self.assertRaisesRegex(ValueError, 'escape'):
            self.run_trace(sky, surface, gap=3)


if __name__ == '__main__':
    unittest.main()
