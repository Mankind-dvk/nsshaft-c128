"""Exact purple spring shapes and lifecycle on compiled 6502 routines.

Run build.ps1 first. Reuses test_charset's py65 machine and mixed-mode pixel
decoder; no VIC raster, MMU, sprite, or SID emulation.
"""
import argparse
from pathlib import Path
import unittest

from test_charset import Machine, ROOT

ARGS = None
PURPLE = 4
RELAXED = [0xff, 0x0c, 0x30, 0xf0, 0x0f, 0x0c, 0x30, 0xff]
COMPRESSED = [0, 0, 0, 0, 0xff, 0xff, 0xff, 0xff]
NORMAL_CODES = [3, 12, 13]
COMPRESSED_CODES = [4, 14, 15]
COMPRESS = dict(zip(NORMAL_CODES, COMPRESSED_CODES))
RESTORE = dict(zip(COMPRESSED_CODES, NORMAL_CODES))


class SpringMaterialTests(unittest.TestCase):
    def new(self):
        return Machine(ARGS.build_dir)

    def game(self, col=4, width=7):
        m = self.new()
        for entry in ['load_game_charset', 'clear_screens_and_colors',
                      'initialize_score', 'initialize_health',
                      'initialize_fade_platforms', 'initialize_spring_platforms',
                      'initialize_conveyor_platforms', 'initialize_ui']:
            m.call(entry)
        m.set('fine_scroll', 7)
        m.set('active_screen', 0)
        m.set('rows_until_platform', 200)
        m.set('platform_type', 3)
        m.set('platform_start', col)
        m.set('platform_width', width)
        address = 0x0400+12*40
        m.mem[0xfb:0xfd] = [address & 255, address >> 8]
        m.call('draw_platform')
        m.call('prepare_screen_b_from_a')
        return m

    def assert_band(self, m, shape, top, col, width):
        pixels = m.pixels()
        for y in range(3*8, 23*8):
            expected = bytes((width+2)*8)
            if 0 <= y-top < 8:
                tile = bytes(PURPLE if shape[y-top] & (128 >> x) else 0
                             for x in range(8))
                expected = bytes(8)+tile*width+bytes(8)
            actual = pixels[y*320+(col-1)*8:y*320+(col+width+1)*8]
            self.assertEqual(actual, expected, (shape, top, col, y))

    def test_exact_source_live_glyphs_and_hires_purple_palette(self):
        m = self.new()
        sources = m.mem[0x4000:0x5000].copy()
        for code, shape in [(3, RELAXED), (4, COMPRESSED)]:
            self.assertEqual(m.mem[0x4000+code*8:0x4008+code*8], shape)
            # Each reference-image block spans two adjacent hires pixels.
            for row in shape:
                self.assertEqual(row & 0x55, (row >> 1) & 0x55)
        m.call('load_game_charset')
        for code, shape in [(3, RELAXED), (4, COMPRESSED)]:
            self.assertEqual(m.mem[0x2800+code*8:0x2808+code*8], shape)
        colors = m.labels['game_character_colors']
        for code in NORMAL_CODES+COMPRESSED_CODES:
            self.assertEqual(m.mem[colors+code], PURPLE, code)
        self.assertEqual(m.mem[m.labels['platform_colors']+3], PURPLE)
        self.assertEqual(m.mem[0xd016] & 16, 16)
        self.assertEqual(m.mem[0x4000:0x5000], sources)

    def test_all_fragment_phases_copy_exact_rows_without_source_mutation(self):
        m = self.new()
        m.call('load_game_charset')
        sources = m.mem[0x4000:0x5000].copy()
        # Phase 7 displays FULL; only phases 0..6 use the fragment builder.
        for phase in range(7):
            m.set('fine_scroll', phase)
            m.call('update_fragment_glyphs')
            split, count = phase+1, 7-phase
            for shape, upper, lower in [(RELAXED, 12, 13), (COMPRESSED, 14, 15)]:
                self.assertEqual(m.mem[0x2800+upper*8:0x2808+upper*8],
                                 [0]*split+shape[:count], (phase, upper))
                self.assertEqual(m.mem[0x2800+lower*8:0x2808+lower*8],
                                 shape[count:]+[0]*count, (phase, lower))
            self.assertEqual(m.mem[0x4000:0x5000], sources, phase)
        self.assertEqual(m.mem[0x2800+3*8:0x2800+4*8], RELAXED)
        self.assertEqual(m.mem[0x2800+4*8:0x2800+5*8], COMPRESSED)

    def test_both_shapes_scroll_one_pixel_without_trails_or_recoloring(self):
        for shape in [RELAXED, COMPRESSED]:
            for col, width in [(4, 7), (15, 12)]:
                m = self.game(col, width)
                if shape == COMPRESSED:
                    m.set('collision_row', 12)
                    m.call('compress_spring_platform')
                for step in range(97):
                    self.assert_band(m, shape, 12*8-step, col, width)
                    m.call('scroll_one_pixel_up')

    def test_compression_and_restore_mapping_is_exhaustive(self):
        m = self.new()
        for mode, mapping in [(0, COMPRESS), (1, RESTORE)]:
            m.set('spring_effect_mode', mode)
            for code in range(256):
                self.assertEqual(m.call('transform_spring_character', a=code),
                                 mapping.get(code, code), (mode, code))

    def test_compress_cancel_and_launch_restore_both_buffers_in_every_phase(self):
        for pixels_scrolled in range(9):
            for finish in ['cancel', 'launch']:
                m = self.game()
                for _ in range(pixels_scrolled):
                    m.call('scroll_one_pixel_up')
                screens = [m.mem[screen:screen+1000].copy()
                           for screen in [0x0400, 0x0c00]]
                colors = m.mem[0xd800:0xdc00].copy()
                m.set('collision_row', 12-pixels_scrolled//8)
                m.set('player_on_platform', 1)
                m.call('compress_spring_platform')
                self.assertEqual(m.get('spring_compression_timer'), 8)
                self.assertEqual(m.get('player_on_spring'), 1)
                compressed = [[COMPRESS.get(code, code) for code in screen]
                              for screen in screens]
                for address, expected in zip([0x0400, 0x0c00], compressed):
                    self.assertEqual(m.mem[address:address+1000], expected,
                                     (pixels_scrolled, finish, address))
                self.assert_band(m, COMPRESSED, 12*8-pixels_scrolled, 4, 7)
                if finish == 'cancel':
                    m.call('cancel_spring_platform')
                    self.assertEqual(m.get('player_rise_velocity'), 0)
                    self.assertEqual(m.get('player_on_platform'), 1)
                else:
                    for remaining in range(7, 0, -1):
                        m.call('update_spring_platform')
                        self.assertEqual(m.get('spring_compression_timer'), remaining)
                        for address, expected in zip([0x0400, 0x0c00], compressed):
                            self.assertEqual(m.mem[address:address+1000], expected)
                    m.call('update_spring_platform')
                    self.assertEqual(m.get('player_rise_velocity'), 24)
                    self.assertEqual(m.get('player_on_platform'), 0)
                self.assertEqual(m.get('spring_compression_timer'), 0)
                self.assertEqual(m.get('player_on_spring'), 0)
                for address, expected in zip([0x0400, 0x0c00], screens):
                    self.assertEqual(m.mem[address:address+1000], expected,
                                     (pixels_scrolled, finish, address))
                self.assertEqual(m.mem[0xd800:0xdc00], colors)
                self.assert_band(m, RELAXED, 12*8-pixels_scrolled, 4, 7)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build')
    ARGS = parser.parse_args()
    unittest.main(argv=['test_spring_material'], verbosity=2)
