"""Reference-image fade stages and 50-tick lifecycle on compiled 6502 routines.

Run build.ps1 first. Reuses test_charset's py65 machine and mixed-mode pixel
decoder; no VIC raster, MMU, sprite, or SID emulation.
"""
import argparse
from pathlib import Path
import unittest

from test_charset import Machine, ROOT

ARGS = None
INTACT = [0xfb, 0xfb, 0xaa, 0xbf, 0xbf, 0xaa, 0xfb, 0xfb]
CRACKED = [0xfb, 0xf3, 0x20, 0x3f, 0x3f, 0x02, 0xf3, 0xfb]
BROKEN = [0xf3, 0xf3, 0, 0x3f, 0x3f, 0, 0xf3, 0xf3]
INTACT_CODES = [56, 57, 58]
CRACKED_CODES = [7, 16, 17]
BROKEN_CODES = [59, 60, 61]
STAGES = [(INTACT, INTACT_CODES), (CRACKED, CRACKED_CODES),
          (BROKEN, BROKEN_CODES)]
ACTIVATE = dict(zip(INTACT_CODES, CRACKED_CODES))
ADVANCE = dict(zip(CRACKED_CODES, BROKEN_CODES))
REMOVE = dict.fromkeys(CRACKED_CODES+BROKEN_CODES, 0)
SCREENS = [0x0400, 0x0c00]
PIXEL_COLORS = [0, 1, 12, 2]


def transform(screen, mapping):
    return [mapping.get(code, code) for code in screen]


class FadeMaterialTests(unittest.TestCase):
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
        self.draw(m, 2, 12, col, width)
        m.call('prepare_screen_b_from_a')
        return m

    def draw(self, m, kind, row, col, width):
        m.set('platform_type', kind)
        m.set('platform_start', col)
        m.set('platform_width', width)
        address = 0x0400+row*40
        m.mem[0xfb:0xfd] = [address & 255, address >> 8]
        m.call('draw_platform')

    def activate(self, m, row=12):
        m.set('collision_row', row)
        m.call('activate_fade_platform')

    def screens(self, m):
        return [m.mem[address:address+1000].copy() for address in SCREENS]

    def assert_screens(self, m, expected, context):
        for address, screen in zip(SCREENS, expected):
            self.assertEqual(m.mem[address:address+1000], screen,
                             (context, address))

    def assert_band(self, m, shape, top, col=4, width=7):
        pixels = m.pixels()
        for y in range(3*8, 23*8):
            expected = bytes((width+2)*8)
            if 0 <= y-top < 8:
                tile = bytes(PIXEL_COLORS[(shape[y-top] >> (6-2*(x//2))) & 3]
                             for x in range(8))
                expected = bytes(8)+tile*width+bytes(8)
            actual = pixels[y*320+(col-1)*8:y*320+(col+width+1)*8]
            self.assertEqual(actual, expected, (shape, top, col, y))

    def test_exact_reference_source_live_glyphs_and_unchanged_shared_palette(self):
        m = self.new()
        sources = m.mem[0x4000:0x5000].copy()
        for shape, codes in STAGES:
            code = codes[0]
            self.assertEqual(m.mem[0x4000+code*8:0x4008+code*8], shape, code)
            for row in shape:
                self.assertNotIn(1, [(row >> shift) & 3 for shift in [6, 4, 2, 0]])
        m.call('load_game_charset')
        colors = m.labels['game_character_colors']
        for shape, codes in STAGES:
            code = codes[0]
            self.assertEqual(m.mem[0x2800+code*8:0x2808+code*8], shape, code)
            for code in codes:
                self.assertEqual(m.mem[colors+code], 10, code)
        self.assertEqual(m.mem[m.labels['platform_colors']+2], 10)
        self.assertEqual(m.mem[0xd021:0xd024], [0, 1, 12])
        self.assertEqual(m.mem[0xd016] & 16, 16)
        self.assertEqual(m.mem[0x4000:0x5000], sources)

    def test_all_fragment_phases_copy_exact_rows_without_source_mutation(self):
        m = self.new()
        m.call('load_game_charset')
        sources = m.mem[0x4000:0x5000].copy()
        # Phase 7 uses FULL codes; phases 0..6 split the shape over two rows.
        for phase in range(7):
            m.set('fine_scroll', phase)
            m.call('update_fragment_glyphs')
            split, count = phase+1, 7-phase
            for shape, codes in STAGES:
                full, upper, lower = codes
                self.assertEqual(m.mem[0x2800+upper*8:0x2808+upper*8],
                                 [0]*split+shape[:count], (phase, upper))
                self.assertEqual(m.mem[0x2800+lower*8:0x2808+lower*8],
                                 shape[count:]+[0]*count, (phase, lower))
                self.assertEqual(m.mem[0x2800+full*8:0x2808+full*8], shape)
            self.assertEqual(m.mem[0x4000:0x5000], sources, phase)

    def test_three_stages_scroll_without_trails_or_recoloring_in_both_buffers(self):
        for stage, (shape, _) in enumerate(STAGES):
            for col, width in [(4, 7), (15, 12)]:
                m = self.game(col, width)
                if stage:
                    self.activate(m)
                if stage == 2:
                    for _ in range(25):
                        m.call('update_fade_platform')
                buffers_seen = set()
                for step in range(97):
                    buffers_seen.add(m.get('active_screen'))
                    self.assert_band(m, shape, 12*8-step, col, width)
                    m.call('scroll_one_pixel_up')
                self.assertEqual(buffers_seen, {0, 1})

    def test_activation_25_tick_stage_change_and_50_tick_removal_every_phase(self):
        for pixels_scrolled in range(17):
            m = self.game()
            for _ in range(pixels_scrolled):
                m.call('scroll_one_pixel_up')
            intact = self.screens(m)
            cracked = [transform(screen, ACTIVATE) for screen in intact]
            broken = [transform(screen, ADVANCE) for screen in cracked]
            removed = [transform(screen, REMOVE) for screen in broken]
            colors = m.mem[0xd800:0xdc00].copy()
            self.activate(m, 12-pixels_scrolled//8)
            self.assertEqual(m.get('fade_platform_timer'), 50)
            self.assertEqual(m.get('fade_stage'), 0)
            self.assertEqual(m.get('fade_platform_active'), 1)
            self.assert_screens(m, cracked, (pixels_scrolled, 0))
            self.assert_band(m, CRACKED, 12*8-pixels_scrolled)
            for tick in range(1, 51):
                m.call('update_fade_platform')
                self.assertEqual(m.get('fade_platform_timer'), 50-tick,
                                 (pixels_scrolled, tick))
                expected = cracked if tick < 25 else broken if tick < 50 else removed
                self.assert_screens(m, expected, (pixels_scrolled, tick))
                if tick < 50:
                    self.assertEqual(m.get('fade_stage'), int(tick >= 25))
                    self.assertEqual(m.get('fade_platform_active'), 1)
                else:
                    self.assertEqual(m.get('fade_platform_active'), 0)
            self.assert_band(m, [0]*8, 12*8-pixels_scrolled)
            self.assertEqual(m.mem[0xd800:0xdc00], colors)
            # Inactive updates must not underflow the completed timer or repaint.
            for _ in range(5):
                m.call('update_fade_platform')
            self.assertEqual(m.get('fade_platform_timer'), 0)
            self.assert_screens(m, removed, (pixels_scrolled, 'after removal'))

    def test_untriggered_fade_and_normal_platforms_are_not_changed(self):
        m = self.game()
        self.draw(m, 0, 8, 4, 7)
        self.draw(m, 2, 16, 17, 7)
        m.call('prepare_screen_b_from_a')
        protected = {}
        for address in SCREENS:
            for row in [8, 16]:
                start = address+row*40
                protected[start] = m.mem[start:start+40].copy()
        palette = m.mem[0xd021:0xd024].copy()
        colors = m.mem[0xd800:0xdc00].copy()
        self.activate(m)
        for tick in range(51):
            for start, expected in protected.items():
                self.assertEqual(m.mem[start:start+40], expected, (tick, start))
            self.assertEqual(m.mem[0xd021:0xd024], palette)
            self.assertEqual(m.mem[0xd800:0xdc00], colors)
            m.call('update_fade_platform')

    def test_removal_releases_only_player_still_supported_by_active_platform(self):
        for supported in [0, 1]:
            m = self.game()
            self.activate(m)
            m.set('player_on_platform', 1)
            m.set('player_on_fade_platform', supported)
            m.set('player_fall_fraction', 5)
            m.set('player_fall_velocity', 7)
            for tick in range(1, 51):
                m.call('update_fade_platform')
                released = supported and tick == 50
                self.assertEqual(m.get('player_on_platform'), 0 if released else 1)
                self.assertEqual(m.get('player_on_fade_platform'),
                                 0 if released else supported)
                self.assertEqual(m.get('player_fall_fraction'), 0 if released else 5)
                self.assertEqual(m.get('player_fall_velocity'), 0 if released else 7)

    def test_new_activation_removes_old_stage_and_starts_fresh_lifetime(self):
        for old_ticks in [7, 30]:
            m = self.game()
            self.draw(m, 2, 20, 17, 7)
            m.call('prepare_screen_b_from_a')
            self.activate(m)
            for _ in range(old_ticks):
                m.call('update_fade_platform')
            m.set('player_on_fade_platform', 0)
            self.activate(m, 20)
            self.assertEqual(m.get('fade_platform_row'), 20)
            self.assertEqual(m.get('fade_platform_timer'), 50)
            self.assertEqual(m.get('fade_stage'), 0)
            self.assertEqual(m.get('fade_platform_active'), 1)
            for address in SCREENS:
                for row in [11, 12, 13]:
                    codes = m.mem[address+row*40:address+(row+1)*40]
                    self.assertTrue(all(code not in CRACKED_CODES+BROKEN_CODES
                                        for code in codes), (old_ticks, address, row))
                codes = m.mem[address+20*40+17:address+20*40+24]
                self.assertEqual(codes, [CRACKED_CODES[0]]*7)
            for _ in range(24):
                m.call('update_fade_platform')
            self.assertEqual(m.get('fade_platform_timer'), 26)
            self.assertEqual(m.get('fade_stage'), 0)
            m.call('update_fade_platform')
            self.assertEqual(m.get('fade_platform_timer'), 25)
            self.assertEqual(m.get('fade_stage'), 1)

    def test_initialization_clears_previous_lifecycle(self):
        m = self.game()
        self.activate(m)
        for _ in range(30):
            m.call('update_fade_platform')
        m.set('player_on_fade_platform', 1)
        m.call('initialize_fade_platforms')
        for name in ['fade_platform_active', 'fade_platform_row',
                     'fade_platform_timer', 'fade_stage', 'player_on_fade_platform']:
            self.assertEqual(m.get(name), 0, name)
        self.assertNotIn('fade_flash_counter', m.labels)
        self.assertNotIn('fade_flash_color', m.labels)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT/'build')
    ARGS = parser.parse_args()
    unittest.main(argv=['test_fade_material'], verbosity=2)
