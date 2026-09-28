"""White/blue conveyor texture, two-axis animation, and existing player push.

Run build.ps1 first. Executes compiled 6502 routines with test_charset's py65
machine and pixel decoder; no VIC raster, MMU, sprite, or SID emulation.
"""
import argparse
from pathlib import Path
import unittest

from test_charset import Machine, ROOT

ARGS = None
PATTERN = [0x5f, 0xd7, 0xf5, 0x7d, 0x7d, 0xf5, 0xd7, 0x5f]
PATTERNS = {'right': PATTERN,
            'left': [0xf5, 0xd7, 0x5f, 0x7d, 0x7d, 0x5f, 0xd7, 0xf5]}
REFERENCE_ROWS = ['WWBB', 'BWWB', 'BBWW', 'WBBW',
                  'WBBW', 'BBWW', 'BWWB', 'WWBB']
DIRECTIONS = [('left', 4, 5, 18, 6, -1), ('right', 5, 6, 19, 7, 1)]
CONVEYOR_CODES = [5, 6, 18, 19, 66, 67]
PIXEL_COLORS = [0, 1, 12, 6]


def rotated(shape, direction, steps):
    shift = (steps % 4)*2
    if not shift:
        return shape.copy()
    if direction == 'left':
        return [((row << shift) | (row >> (8-shift))) & 255 for row in shape]
    return [((row >> shift) | (row << (8-shift))) & 255 for row in shape]


class ConveyorMaterialTests(unittest.TestCase):
    def new(self):
        return Machine(ARGS.build_dir)

    def game(self, kind=4, col=4, width=7):
        m = self.new()
        m.set('fine_scroll', 7)
        m.set('active_screen', 0)
        for entry in ['load_game_charset', 'clear_screens_and_colors',
                      'initialize_score', 'initialize_health',
                      'initialize_fade_platforms', 'initialize_spring_platforms',
                      'initialize_conveyor_platforms', 'initialize_ui']:
            m.call(entry)
        m.set('rows_until_platform', 200)
        m.set('platform_type', kind)
        m.set('platform_start', col)
        m.set('platform_width', width)
        address = 0x0400+12*40
        m.mem[0xfb:0xfd] = [address & 255, address >> 8]
        m.call('draw_platform')
        m.call('prepare_screen_b_from_a')
        return m

    def pattern(self, m, direction):
        address = m.labels[f'conveyor_{direction}_pattern']
        return m.mem[address:address+8]

    def glyph(self, m, code):
        return m.mem[0x2800+code*8:0x2808+code*8]

    def assert_fragments(self, m, phase, steps):
        for direction, _, full, upper, _, _ in DIRECTIONS:
            shape = rotated(PATTERNS[direction], direction, steps)
            self.assertEqual(self.pattern(m, direction), shape,
                             (direction, phase, steps, 'pattern'))
            if phase == 7:
                self.assertEqual(self.glyph(m, full), shape,
                                 (direction, phase, steps, 'full'))
            else:
                split, count = phase+1, 7-phase
                self.assertEqual(self.glyph(m, upper), [0]*split+shape[:count],
                                 (direction, phase, steps, 'upper'))
                lower = 66 if direction == 'left' else 67
                self.assertEqual(self.glyph(m, lower), shape[count:]+[0]*count,
                                 (direction, phase, steps, 'lower'))

    def assert_band(self, m, shape, top, col, width):
        pixels = m.pixels()
        for y in range(3*8, 23*8):
            expected = bytes((width+2)*8)
            if 0 <= y-top < 8:
                tile = bytes(PIXEL_COLORS[(shape[y-top] >> (6-2*(x//2))) & 3]
                             for x in range(8))
                expected = bytes(8)+tile*width+bytes(8)
            actual = pixels[y*320+(col-1)*8:y*320+(col+width+1)*8]
            self.assertEqual(actual, expected, (shape, top, col, y))

    def test_exact_reference_source_live_glyphs_and_multicolor_palette(self):
        m = self.new()
        sources = m.mem[0x4000:0x5000].copy()
        for row, expected in zip(PATTERN, REFERENCE_ROWS):
            self.assertEqual(''.join({1: 'W', 3: 'B'}[(row >> shift) & 3]
                                     for shift in [6, 4, 2, 0]), expected)
        for direction, _, code, _, _, _ in DIRECTIONS:
            self.assertEqual(m.mem[0x4000+code*8:0x4008+code*8], PATTERNS[direction])
        m.call('load_game_charset')
        m.set('fine_scroll', 7)
        m.call('initialize_conveyor_platforms')
        for direction, kind, full, upper, _, _ in DIRECTIONS:
            self.assertEqual(self.pattern(m, direction), PATTERNS[direction])
            self.assertEqual(self.glyph(m, full), PATTERNS[direction])
            self.assertEqual(self.glyph(m, upper), [0]*8)
            self.assertEqual(m.mem[m.labels['platform_colors']+kind], 14)
        for code in CONVEYOR_CODES:
            self.assertEqual(m.mem[m.labels['game_character_colors']+code], 14)
        self.assertEqual(m.mem[0xd021:0xd024], [0, 1, 12])
        self.assertEqual(m.mem[0xd016] & 16, 16)
        self.assertEqual(m.mem[0x4000:0x5000], sources)

    def test_left_is_horizontal_mirror_without_swapping_color_bits(self):
        m = self.game()
        for frame in range(13):
            for left, right in zip(self.pattern(m, 'left'), self.pattern(m, 'right')):
                left_pixels = [(left >> shift) & 3 for shift in [6, 4, 2, 0]]
                right_pixels = [(right >> shift) & 3 for shift in [6, 4, 2, 0]]
                self.assertEqual(left_pixels, right_pixels[::-1], frame)
            m.call('update_conveyor_animation')

    def test_three_frame_cadence_and_opposite_four_phase_cycles(self):
        m = self.game()
        self.assertEqual(m.get('conveyor_animation_counter'), 3)
        for frame in range(25):
            self.assert_fragments(m, 7, frame//3)
            self.assertEqual(m.get('conveyor_animation_counter'), 3-frame % 3,
                             frame)
            m.call('update_conveyor_animation')

    def test_horizontal_animation_rebuilds_every_vertical_fragment_phase(self):
        for phase in range(8):
            m = self.game()
            sources = m.mem[0x4000:0x5000].copy()
            m.set('fine_scroll', phase)
            if phase != 7:
                m.call('update_fragment_glyphs')
            for frame in range(13):
                self.assert_fragments(m, phase, frame//3)
                self.assertEqual(m.mem[0x4000:0x5000], sources, (phase, frame))
                m.call('update_conveyor_animation')

    def test_simultaneous_upward_and_horizontal_scroll_has_no_trails_or_reset(self):
        for direction, kind, _, _, _, _ in DIRECTIONS:
            for col, width in [(4, 7), (15, 12)]:
                m = self.game(kind, col, width)
                sources = m.mem[0x4000:0x5000].copy()
                buffers_seen = set()
                for step in range(97):
                    buffers_seen.add(m.get('active_screen'))
                    self.assert_fragments(m, m.get('fine_scroll'), step//3)
                    self.assert_band(m, rotated(PATTERNS[direction], direction, step//3),
                                     12*8-step, col, width)
                    m.call('update_conveyor_animation')
                    counter = m.get('conveyor_animation_counter')
                    patterns = [self.pattern(m, d).copy() for d in ['left', 'right']]
                    m.call('scroll_one_pixel_up')
                    self.assertEqual(m.get('conveyor_animation_counter'), counter)
                    self.assertEqual([self.pattern(m, d) for d in ['left', 'right']],
                                     patterns, (direction, step))
                self.assertEqual(buffers_seen, {0, 1})
                self.assertEqual(m.mem[0x4000:0x5000], sources)

    def test_unoccupied_animation_preserves_geometry_colors_and_other_glyphs(self):
        m = self.game()
        m.set('player_on_platform', 0)
        m.set('player_x_low', 123)
        m.set('player_x_high', 0)
        m.set('player_y', 140)
        m.set('conveyor_move_counter', 2)
        for phase in range(8):
            m.set('fine_scroll', phase)
            if phase != 7:
                m.call('update_fragment_glyphs')
            else:
                m.call('restore_conveyor_full_glyphs')
            screens = [m.mem[a:a+1000].copy() for a in [0x0400, 0x0c00]]
            colors = m.mem[0xd800:0xdc00].copy()
            glyphs = {code: self.glyph(m, code).copy()
                      for code in range(68) if code not in CONVEYOR_CODES}
            patterns = [self.pattern(m, d).copy() for d in ['left', 'right']]
            for _ in range(3):
                m.call('update_conveyor_animation')
            self.assertNotEqual([self.pattern(m, d) for d in ['left', 'right']],
                                patterns, phase)
            self.assertEqual([m.mem[a:a+1000] for a in [0x0400, 0x0c00]], screens)
            self.assertEqual(m.mem[0xd800:0xdc00], colors)
            for code, expected in glyphs.items():
                self.assertEqual(self.glyph(m, code), expected, (phase, code))
            for name, expected in [('player_on_platform', 0), ('player_x_low', 123),
                                   ('player_x_high', 0), ('player_y', 140),
                                   ('conveyor_move_counter', 2)]:
                self.assertEqual(m.get(name), expected, (phase, name))

    def test_round_initialization_restores_pattern_counter_and_empty_uppers(self):
        m = self.game()
        for frame in range(3, 12):
            for _ in range(frame):
                m.call('update_conveyor_animation')
            m.set('fine_scroll', frame % 7)
            m.call('update_fragment_glyphs')
            m.set('fine_scroll', 7)
            m.call('initialize_conveyor_platforms')
            self.assert_fragments(m, 7, 0)
            self.assertEqual(m.get('conveyor_animation_counter'), 3)
            for fragment in [18, 19, 66, 67]:
                self.assertEqual(self.glyph(m, fragment), [0]*8)

    def test_player_push_remains_two_pixels_per_three_calls_independent_of_animation(self):
        for direction, _, _, _, collision, sign in DIRECTIONS:
            m = self.game()
            m.set('collision_type', collision)
            m.set('player_x_low', 120)
            m.set('player_x_high', 0)
            m.set('conveyor_move_counter', 0)
            m.set('player_move_counter', 1)
            for frame in range(12):
                m.call('update_conveyor_animation')
                counter = m.get('conveyor_animation_counter')
                m.call('apply_conveyor_motion')
                expected = 120+sign*2*(frame//3+1)
                self.assertEqual(m.get('player_x_low'), expected, (direction, frame))
                self.assertEqual(m.get('player_x_high'), 0)
                self.assertEqual(m.mem[0xd000], expected)
                self.assertEqual(m.get('conveyor_move_counter'), 2-frame % 3)
                self.assertEqual(m.get('conveyor_animation_counter'), counter)
                self.assertEqual(m.get('player_move_counter'), 1)

    def test_player_push_keeps_playfield_boundary_clamps(self):
        for collision, start, expected in [(6, 40, 40), (6, 41, 40), (6, 42, 40),
                                           (7, 230, 232), (7, 231, 232),
                                           (7, 232, 232)]:
            m = self.game()
            m.set('collision_type', collision)
            m.set('player_x_low', start)
            m.set('player_x_high', 0)
            m.set('conveyor_move_counter', 0)
            m.call('apply_conveyor_motion')
            self.assertEqual(m.get('player_x_low'), expected, (collision, start))
            self.assertEqual(m.get('player_x_high'), 0)
            self.assertEqual(m.mem[0xd000], expected)
            self.assertEqual(m.mem[0xd010] & 1, 0)

    def test_collision_support_stays_at_platform_top_in_every_animation_phase(self):
        for direction, kind, _, _, collision, _ in DIRECTIONS:
            m = self.game(kind)
            for step in range(17):
                for frame in range(12):
                    for x in [24, 72, 120]:
                        m.set('player_x_low', x)
                        m.set('player_x_high', 0)
                        for delta in [-8, -1, 0, 1, 2, 3, 8]:
                            m.set('player_y', 50+12*8-step-21+delta)
                            expected = collision if x == 72 and delta in [0, 1, 2] else 0
                            self.assertEqual(m.call('find_platform_below_player'),
                                             expected, (direction, step, frame, x, delta))
                    m.call('update_conveyor_animation')
                m.call('scroll_one_pixel_up')

    def test_compiled_main_loop_calls_animation_once_per_frame(self):
        m = self.new()
        loop = bytes(m.mem[m.labels['main_loop']:m.labels['game_over_screen']])
        target = m.labels['update_conveyor_animation']
        call = bytes([0x20, target & 255, target >> 8])
        self.assertEqual(loop.count(call), 1)
        scroll = m.labels['scroll_step_ready']
        scroll_call = bytes([0x20, scroll & 255, scroll >> 8])
        self.assertLess(loop.index(call), loop.index(scroll_call))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build')
    ARGS = parser.parse_args()
    unittest.main(argv=['test_conveyor_material'], verbosity=2)
