"""Raw-pixel brick scrolling and mixed-mode checks on compiled 6502 routines.

No VIC bus stealing, raster IRQ, MMU or SID emulation. Optional --baseline-dir
accepts the PRG/labels saved immediately before the brick-platform change.
"""
import argparse
from pathlib import Path
import unittest

from test_charset import Machine, ROOT

ARGS = None
PATTERN = ['RRWR', 'RRWR', 'WWWW', 'WRRR',
           'WRRR', 'WWWW', 'RRWR', 'RRWR']
TILE = [bytes(c for block in row for c in [2 if block == 'R' else 1]*2)
        for row in PATTERN]


class BrickPlatformTests(unittest.TestCase):
    def game(self):
        m = Machine(ROOT/'build')
        for entry in ['load_game_charset', 'clear_screens_and_colors',
                      'initialize_score', 'initialize_health',
                      'initialize_fade_platforms', 'initialize_spring_platforms',
                      'initialize_conveyor_platforms', 'initialize_ui']:
            m.call(entry)
        m.set('fine_scroll', 7)
        m.set('active_screen', 0)
        m.set('rows_until_platform', 200)
        return m

    def draw(self, m, kind, row, col, width):
        m.set('platform_type', kind)
        m.set('platform_start', col)
        m.set('platform_width', width)
        address = 0x0400+row*40
        m.mem[0xfb:0xfd] = [address & 255, address >> 8]
        m.call('draw_platform')

    def assert_brick_band(self, m, top, col, width):
        pixels = m.pixels()
        for y in range(3*8, 23*8):
            actual = pixels[y*320+(col-1)*8:y*320+(col+width+1)*8]
            expected = bytes((width+2)*8)
            if 0 <= y-top < 8:
                expected = bytes(8)+TILE[y-top]*width+bytes(8)
            self.assertEqual(actual, expected, (top, y))

    def test_exact_pattern_moves_one_pixel_without_rectangular_trails(self):
        for col, width in [(4, 7), (15, 12)]:
            m = self.game()
            self.draw(m, 0, 12, col, width)
            m.call('prepare_screen_b_from_a')
            for step in range(97):
                self.assert_brick_band(m, 12*8-step, col, width)
                m.call('scroll_one_pixel_up')

    def test_fade_crumbling_does_not_recolor_bricks(self):
        m = self.game()
        self.draw(m, 0, 9, 4, 7)
        self.draw(m, 2, 15, 17, 7)
        m.call('prepare_screen_b_from_a')
        m.set('collision_row', 15)
        m.call('activate_fade_platform')
        for step in range(33):
            self.assert_brick_band(m, 9*8-step, 4, 7)
            m.call('update_fade_platform')
            m.call('scroll_one_pixel_up')

    def test_spawn_and_scene_modes(self):
        m = self.game()
        m.call('initialize_platform_generation')
        m.set('random_state_low', 0x34)
        m.set('random_state_high', 0x12)
        m.call('seed_platforms')
        start, width = m.get('platform_start'), m.get('platform_width')
        m.call('initialize_player')
        m.call('prepare_screen_b_from_a')
        self.assertEqual(m.get('player_on_platform'), 1)
        self.assertEqual(m.mem[0x4008:0x4010], [0xf7, 0xf7, 0x55, 0x7f, 0x7f, 0x55, 0xf7, 0xf7])
        for screen in [0x0400, 0x0c00]:
            self.assertEqual(m.mem[screen+22*40+start:screen+22*40+start+width], [1]*width)
        self.assertEqual(m.mem[0xd800+22*40+start:0xd800+22*40+start+width], [10]*width)
        for scene in ['show_start_screen', 'show_game_over_screen', 'load_game_charset']:
            m.call(scene)
            self.assertEqual(m.mem[0xd011] & 0x67, 3)  # No ECM/bitmap; phase 3.
            self.assertEqual(m.mem[0xd016], 0x18)
            self.assertEqual(m.mem[0xd021:0xd024], [0, 1, 12])
        self.assertEqual(m.mem[0x2800+40*8:0x2800+50*8], m.mem[0x4800+48*8:0x4800+58*8])

    def test_eight_pixel_cycle_does_not_cost_more_than_baseline(self):
        if ARGS.baseline_dir is None:
            self.skipTest('Pass --baseline-dir with the pre-brick-platform build')
        totals = []
        for directory in [ARGS.baseline_dir, ROOT/'build']:
            m = Machine(directory)
            m.setup_game()
            start = m.cpu.processorCycles
            for _ in range(8):
                m.call('scroll_one_pixel_up')
            totals.append(m.cpu.processorCycles-start)
        self.assertLessEqual(totals[1], totals[0], totals)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-dir', type=Path)
    ARGS = parser.parse_args()
    unittest.main(argv=['test_brick_platforms'], verbosity=2)
