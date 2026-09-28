"""Exact white/green spike pixels, vertical flipping and palette isolation.

Compiled 6502 routines only, not VIC raster/MMU/SID emulation. Optional
--baseline-dir compares everything except the changed spike pixels with the
PRG/labels saved immediately before the white/green spike change.
"""
import argparse
from pathlib import Path
import unittest

from test_charset import Machine, ROOT

ARGS = None
PATTERN = ['..W.', '..W.', '..G.', '.GWW', '.WGW', '.WWG', 'GWGW', 'WGWG']
COLORS = {'.': 0, 'W': 1, 'G': 5}
TILE = [bytes(COLORS[c] for c in row for _ in range(2)) for row in PATTERN]


class SpikeMaterialTests(unittest.TestCase):
    def test_ceiling_is_exact_vertical_flip(self):
        m = Machine(ROOT/'build')
        m.setup_game()
        upright = m.mem[0x2800+2*8:0x2800+3*8]
        self.assertEqual(upright, [4, 4, 12, 0x35, 0x1d, 0x17, 0xdd, 0x77])
        self.assertEqual(m.mem[0x2800+21*8:0x2800+22*8], upright[::-1])
        pixels = m.pixels()
        for gy in range(8):
            self.assertEqual(pixels[(16+gy)*320+16:(16+gy)*320+29*8], TILE[7-gy]*27)

    def test_pattern_scrolls_without_trails_in_every_phase(self):
        for col, width in [(4, 8), (17, 7)]:
            m = Machine(ROOT/'build')
            m.setup_game()
            m.call('clear_screens_and_colors')
            m.call('initialize_ui')
            m.set('platform_type', 1)
            m.set('platform_start', col)
            m.set('platform_width', width)
            address = 0x0400+12*40
            m.mem[0xfb:0xfd] = [address & 255, address >> 8]
            m.call('draw_platform')
            m.call('prepare_screen_b_from_a')
            for step in range(97):
                pixels = m.pixels()
                top = 12*8-step
                for y in range(24, 23*8):
                    expected = bytes((width+2)*8)
                    if 0 <= y-top < 8:
                        expected = bytes(8)+TILE[y-top]*width+bytes(8)
                    actual = pixels[y*320+(col-1)*8:y*320+(col+width+1)*8]
                    self.assertEqual(actual, expected, (col, step, y))
                m.call('scroll_one_pixel_up')

    def test_palette_change_preserves_all_other_materials_and_sprites(self):
        if ARGS.baseline_dir is None:
            self.skipTest('Pass --baseline-dir with the pre-white/green-spike build')
        old, new = Machine(ARGS.baseline_dir), Machine(ROOT/'build')
        for m in [old, new]:
            m.setup_game()
            m.set('collision_row', 10)
            m.call('activate_fade_platform')
        for step in range(49):
            expected, actual = bytearray(old.pixels()), new.pixels()
            # Exclude only the fixed ceiling and the known moving spike rectangle.
            for y in range(16, 24):
                expected[y*320+16:y*320+29*8] = actual[y*320+16:y*320+29*8]
            for y in range(max(24, 7*8-step), max(24, 8*8-step)):
                expected[y*320+4*8:y*320+12*8] = actual[y*320+4*8:y*320+12*8]
            self.assertEqual(bytes(expected), actual, step)
            for start, end in [(0x3000, 0x3100), (0x3a00, 0x3cc0)]:
                self.assertEqual(old.mem[start:end], new.mem[start:end])
            for m in [old, new]:
                m.call('update_fade_platform')
                m.call('scroll_one_pixel_up')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', type=Path)
    ARGS = parser.parse_args()
    unittest.main(argv=['test_spike_material'], verbosity=2)
