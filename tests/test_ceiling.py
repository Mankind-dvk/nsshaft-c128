"""Compiled-routine checks for the brick frame and HP-absorbed ceiling hits.

Run build.ps1, then python tests/test_ceiling.py (requires py65).
This does not emulate VIC timing, MMU, SID, or real paddle input.
"""
from pathlib import Path
import unittest

from test_charset import Machine


ROOT = Path(__file__).resolve().parents[1]
CEILING_ROW = 2
CEILING_CODE = 21
TIP_Y = 50 + (CEILING_ROW+1)*8 - 1
DROP_Y = TIP_Y + 8
FRAME_CODE = 20
FRAME_PATTERN = ['RRWR', 'RRWR', 'WWWW', 'WRRR',
                 'WRRR', 'WWWW', 'RRWR', 'RRWR']


class CeilingTests(unittest.TestCase):
    def game(self):
        machine = Machine(ROOT/'build')
        machine.setup_game()
        return machine

    def test_brick_frame_and_inverted_geometry(self):
        m = self.game()
        pixels = m.pixels()
        for row in range(1, 24):
            for col in range(1, 39):
                if row in [1, 23] or col in [1, 29, 38]:
                    for screen in [0x0400, 0x0c00]:
                        self.assertEqual(m.mem[screen+row*40+col], FRAME_CODE)
                    self.assertEqual(m.mem[0xd800+row*40+col], 10)
                    for gy, pattern in enumerate(FRAME_PATTERN):
                        offset = (row*8+gy)*320+col*8
                        expected = bytes(color for block in pattern
                                         for color in [2 if block == 'R' else 1]*2)
                        self.assertEqual(pixels[offset:offset+8], expected)
        self.assertEqual(m.mem[0x2800+21*8:0x2800+22*8],
                         [0x77, 0xdd, 0x17, 0x1d, 0x35, 0x0c, 0x04, 0x04])
        self.assertEqual(TIP_Y, 73)
        self.assertEqual(m.mem[0x0400+2*40+30:0x0400+2*40+35], [52, 51, 53, 54, 55])
        self.assertEqual(m.labels['platform_score_rows_end']-m.labels['platform_score_rows'], 20)

    def test_ceiling_stays_fixed_during_all_scroll_phases(self):
        m = self.game()
        initial_glyph = m.mem[0x2800+21*8:0x2800+22*8].copy()
        for step in range(257):
            for screen in [0x0400, 0x0c00]:
                self.assertEqual(m.mem[screen+2*40+2:screen+2*40+29], [CEILING_CODE]*27, step)
                for row in range(1, 24):
                    columns = range(1, 39) if row in [1, 23] else [1, 29, 38]
                    for col in columns:
                        self.assertEqual(m.mem[screen+row*40+col], FRAME_CODE, step)
            self.assertEqual(m.mem[0xd800+2*40+2:0xd800+2*40+29], [13]*27)
            self.assertEqual(m.mem[0x2800+21*8:0x2800+22*8], initial_glyph)
            m.call('scroll_one_pixel_up')

    def assert_absorbed(self, m, previous_hp):
        self.assertEqual(m.get('game_over_flag'), 0)
        self.assertEqual(m.get('health_points'), previous_hp-1)
        self.assertEqual(m.get('player_y'), DROP_Y)
        self.assertEqual(m.mem[0xd001], DROP_Y)
        self.assertEqual(m.get('player_hurt_timer'), 16)
        for state in ['player_on_platform', 'player_on_fade_platform',
                      'player_on_spring', 'player_fall_fraction',
                      'player_fall_velocity', 'player_rise_velocity',
                      'conveyor_move_counter']:
            self.assertEqual(m.get(state), 0, state)
        digits = [40+int(d) for d in f'{previous_hp-1:03d}']
        for screen in [0x0400, 0x0c00]:
            self.assertEqual(m.mem[screen+6*40+34:screen+6*40+37], digits)

    def test_tip_contact_uses_hp_in_every_vertical_state(self):
        for hp in [0, 1, 3, 255]:
            for supported, rise in [(1, 0), (0, 24), (0, 0)]:
                for y in [TIP_Y, TIP_Y-1]:
                    m = self.game()
                    m.set('health_points', hp)
                    m.set('player_hurt_timer', 12)
                    m.set('player_y', y)
                    m.set('player_on_platform', supported)
                    m.set('player_rise_velocity', rise)
                    m.set('player_fall_velocity', 8)
                    m.set('player_fall_fraction', 7)
                    m.set('conveyor_move_counter', 2)
                    m.call('update_player_vertical')
                    if hp:
                        self.assert_absorbed(m, hp)
                    else:
                        self.assertEqual(m.get('game_over_flag'), 1, (supported, rise, y))
                        self.assertEqual(m.get('health_points'), 0)
                        self.assertEqual(m.get('player_y'), y)
                    self.assertEqual(m.mem[m.labels['score_digits']:m.labels['score_digits']+6], [0]*6)

    def test_one_pixel_safe_margin_and_same_step_contact(self):
        m = self.game()
        m.set('player_y', TIP_Y+1)
        m.set('player_on_platform', 0)
        m.set('player_rise_velocity', 0)
        m.call('update_player_vertical')
        self.assertEqual(m.get('game_over_flag'), 0)

        for entry, supported, rise in [('scroll_one_pixel_up', 1, 0),
                                        ('update_player_vertical', 0, 24)]:
            for hp in [0, 1, 3]:
                m = self.game()
                m.set('player_y', TIP_Y+1)
                m.set('player_on_platform', supported)
                m.set('player_rise_velocity', rise)
                m.set('health_points', hp)
                m.call(entry)
                if hp:
                    self.assert_absorbed(m, hp)
                else:
                    self.assertEqual(m.get('player_y'), TIP_Y)
                    self.assertEqual(m.get('game_over_flag'), 1)
                    self.assertEqual(m.get('health_points'), 0)

    def test_drop_cancels_spring_and_does_not_repeat_damage(self):
        m = self.game()
        m.set('collision_row', 13)
        m.call('compress_spring_platform')
        self.assertEqual(m.get('player_on_spring'), 1)
        for screen in [0x0400, 0x0c00]:
            self.assertEqual(m.mem[screen+13*40+6], 4)
        m.set('player_on_platform', 1)
        m.set('player_y', TIP_Y)
        m.set('health_points', 1)
        m.call('update_player_vertical')
        self.assert_absorbed(m, 1)
        self.assertEqual(m.get('spring_compression_timer'), 0)
        for screen in [0x0400, 0x0c00]:
            self.assertEqual(m.mem[screen+13*40+6], 3)

        # Fall down an empty column, through and beyond the hurt animation.
        m.set('player_x_low', 24+26*8)
        m.set('player_x_high', 0)
        for _ in range(20):
            for entry in ['update_player_vertical', 'update_fade_platform',
                          'update_spring_platform', 'update_player_sprite_frame',
                          'scroll_one_pixel_up']:
                m.call(entry)
            self.assertEqual(m.get('game_over_flag'), 0)
            self.assertEqual(m.get('health_points'), 0)
            self.assertEqual(m.get('player_on_platform'), 0)
            self.assertEqual(m.get('player_rise_velocity'), 0)
        self.assertGreater(m.get('player_y'), DROP_Y)

        # Spending the final HP saves that hit, not the next separate contact.
        m.set('player_y', TIP_Y)
        m.call('update_player_vertical')
        self.assertEqual(m.get('game_over_flag'), 1)
        self.assertEqual(m.get('health_points'), 0)

    def test_ceiling_damage_selects_hurt_frame_for_every_skin(self):
        for skin in range(3):
            m = self.game()
            m.set('selected_character', skin)
            m.set('health_points', 2)
            m.set('player_y', TIP_Y)
            m.set('player_on_fade_platform', 1)
            m.call('update_player_vertical')
            self.assert_absorbed(m, 2)
            m.call('update_player_sprite_frame')
            hurt_block = m.mem[m.labels['character_hurt_blocks']+skin]
            for screen in [0x0400, 0x0c00]:
                self.assertEqual(m.mem[screen+0x3f8], hurt_block)

    def test_floor_spikes_keep_hp_rule_and_bottom_remains_fatal(self):
        m = self.game()
        m.set('health_points', 1)
        m.call('handle_spike_damage')
        self.assertEqual(m.get('health_points'), 0)
        self.assertEqual(m.get('game_over_flag'), 0)
        self.assertEqual(m.get('player_rise_velocity'), 16)
        self.assertEqual(m.get('player_hurt_timer'), 16)

        for hp in [0, 3]:
            m = self.game()
            m.set('health_points', hp)
            m.set('player_y', 214)
            m.set('player_on_platform', 0)
            m.call('update_player_vertical')
            self.assertEqual(m.get('game_over_flag'), 1)
            self.assertEqual(m.get('health_points'), hp)

    def test_seed_rows_and_score_claims_exclude_ceiling(self):
        m = Machine(ROOT/'build')
        for entry in ['load_game_charset', 'clear_screens_and_colors',
                      'initialize_score', 'initialize_platform_generation']:
            m.call(entry)
        m.set('random_state_low', 0x34)
        m.set('random_state_high', 0x12)
        m.call('seed_platforms')
        rows = m.labels['platform_score_rows']
        expected = [0]*20
        for index in [0, 4, 9, 14]:
            expected[index] = 1
        expected[19] = 0x81
        self.assertEqual(m.mem[rows:rows+20], expected)
        self.assertEqual(m.mem[0x0400+2*40:0x0400+3*40], [0]*40)
        m.call('initialize_ui')
        m.set('collision_row', CEILING_ROW)
        m.call('award_platform_landing_score')
        self.assertEqual(m.mem[m.labels['score_digits']:m.labels['score_digits']+6], [0]*6)
        self.assertEqual(m.mem[rows:rows+20], expected)


if __name__ == '__main__':
    unittest.main(verbosity=2)
