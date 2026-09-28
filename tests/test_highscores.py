"""Leaderboard and name-entry regressions on compiled 6502 routines.

Run build.ps1 first. Uses the existing py65 harness; disk I/O, MMU switching,
keyboard hardware, and VIC raster timing require separate integration tests.
"""
import argparse
from pathlib import Path
import random
import unittest

from test_charset import Machine, ROOT


ARGS = None
CAPACITY = 5
NAME_LENGTH = 8
RECORD_LENGTH = 14


def screen_codes(text):
    return [ord(char) & 63 for char in text]


def score_digits(score):
    return [int(digit) for digit in f'{score:06d}']


def record(name, score):
    return screen_codes(name.ljust(NAME_LENGTH)) + score_digits(score)


class HighscoreTests(unittest.TestCase):
    def new(self):
        directory = ARGS.build_dir if ARGS else ROOT / 'build'
        machine = Machine(directory)
        machine.call('initialize_highscores')
        return machine

    def put(self, m, label, values):
        address = m.labels[label]
        m.mem[address:address+len(values)] = values

    def read(self, m, label, length):
        address = m.labels[label]
        return m.mem[address:address+length]

    def set_table(self, m, entries):
        data = [value for name, score in entries for value in record(name, score)]
        self.put(m, 'highscore_entries', data + [0] * (70-len(data)))
        m.set('highscore_count', len(entries))

    def table(self, m):
        data = self.read(m, 'highscore_entries', 70)
        return [data[index:index+RECORD_LENGTH]
                for index in range(0, m.get('highscore_count')*RECORD_LENGTH,
                                   RECORD_LENGTH)]

    def rank(self, m, score):
        self.put(m, 'score_digits', score_digits(score))
        m.call('highscore_find_rank')
        return m.get('highscore_rank')

    def insert(self, m, name, score):
        rank = self.rank(m, score)
        if rank != 0xff:
            self.put(m, 'highscore_name', screen_codes(name.ljust(NAME_LENGTH)))
            m.call('highscore_insert')
        return rank

    def name(self, m):
        return self.read(m, 'highscore_name', NAME_LENGTH)

    def key(self, m, petscii):
        m.call('highscore_name_key', a=petscii)
        return bool(m.cpu.p & m.cpu.CARRY)

    def test_initialization_clears_count_rank_and_unsaved_state(self):
        m = self.new()
        self.set_table(m, [('ALICE', 999999), ('BOB', 120)])
        m.set('highscore_rank', 2)
        m.set('highscore_dirty', 1)
        m.call('initialize_highscores')
        self.assertEqual(m.get('highscore_count'), 0)
        self.assertEqual(m.get('highscore_rank'), 0xff)
        self.assertEqual(m.get('highscore_dirty'), 0)
        self.assertEqual(self.read(m, 'highscore_entries', 70), [0]*70)

    def test_empty_table_accepts_zero_and_maximum_score(self):
        for score in [0, 1, 999999]:
            with self.subTest(score=score):
                self.assertEqual(self.rank(self.new(), score), 0)

    def test_full_table_ranks_and_ties_are_old_first(self):
        m = self.new()
        self.set_table(m, [('A', 128), ('B', 96), ('C', 42),
                          ('D', 25), ('E', 10)])
        for score, expected in [(999999, 0), (129, 0), (128, 1), (97, 1),
                                (96, 2), (80, 2), (42, 3), (26, 3),
                                (25, 4), (11, 4), (10, 0xff), (0, 0xff)]:
            with self.subTest(score=score):
                self.assertEqual(self.rank(m, score), expected)

    def test_unfilled_table_appends_after_existing_equal_scores(self):
        m = self.new()
        self.set_table(m, [('FIRST', 42), ('SECOND', 42), ('ZERO', 0)])
        self.assertEqual(self.rank(m, 42), 2)
        self.assertEqual(self.rank(m, 0), 3)
        self.set_table(m, [('A', 42), ('B', 42), ('C', 42),
                          ('D', 42), ('E', 42)])
        self.assertEqual(self.rank(m, 42), 0xff)

    def test_rank_checks_do_not_mutate_records_score_or_dirty_flag(self):
        m = self.new()
        self.set_table(m, [('FIRST', 100000), ('SECOND', 99999), ('THIRD', 1)])
        before = self.read(m, 'highscore_entries', 70)
        for dirty in [0, 1]:
            m.set('highscore_dirty', dirty)
            for score in [0, 1, 99999, 100000, 999999]:
                self.rank(m, score)
                self.assertEqual(self.read(m, 'highscore_entries', 70), before)
                self.assertEqual(m.get('highscore_count'), 3)
                self.assertEqual(m.get('highscore_dirty'), dirty)
                self.assertEqual(self.read(m, 'score_digits', 6), score_digits(score))

    def test_comparison_uses_all_six_digits_most_significant_first(self):
        for pivot in [1, 10, 100, 1000, 10000, 100000, 999998]:
            with self.subTest(pivot=pivot):
                m = self.new()
                self.set_table(m, [('SCORE', pivot)] * CAPACITY)
                self.assertEqual(self.rank(m, pivot+1), 0)
                self.assertEqual(self.rank(m, pivot), 0xff)
                self.assertEqual(self.rank(m, pivot-1), 0xff)

    def test_insertion_moves_whole_records_and_drops_only_last(self):
        initial = [('FIRST', 128), ('SECOND', 96), ('THIRD', 42),
                   ('FOURTH', 25), ('LAST', 10)]
        for new_score, expected_rank in [(999999, 0), (100, 1), (80, 2),
                                         (30, 3), (11, 4)]:
            with self.subTest(rank=expected_rank):
                m = self.new()
                self.set_table(m, initial)
                self.assertEqual(self.insert(m, 'NEW 1234', new_score), expected_rank)
                expected = initial.copy()
                expected.insert(expected_rank, ('NEW 1234', new_score))
                self.assertEqual(self.table(m), [record(*entry) for entry in expected[:5]])
                self.assertEqual(m.get('highscore_count'), CAPACITY)
                self.assertEqual(m.get('highscore_rank'), expected_rank)
                self.assertEqual(m.get('highscore_dirty'), 1)

    def test_multiple_rounds_match_stable_top_five_reference(self):
        m = self.new()
        scores = [0, 0, 42, 42, 42, 42, 42, 999999, 999999, 10]
        rng = random.Random(128)
        scores += [rng.randrange(1000000) for _ in range(60)]
        expected = []
        for index, score in enumerate(scores):
            name = f'P{index:07d}'
            previous = expected.copy()
            expected = sorted(expected+[(name, score)], key=lambda item: -item[1])[:5]
            m.set('highscore_dirty', 0)
            rank = self.insert(m, name, score)
            with self.subTest(round=index, score=score):
                self.assertEqual(self.table(m), [record(*entry) for entry in expected])
                self.assertEqual(m.get('highscore_count'), len(expected))
                self.assertEqual(m.get('highscore_dirty'), int(expected != previous))
                self.assertEqual(rank, next((i for i, item in enumerate(expected)
                                             if item[0] == name), 0xff))

    def test_new_game_score_reset_preserves_leaderboard(self):
        m = self.new()
        self.insert(m, 'ALICE', 999999)
        self.insert(m, 'BOB', 42)
        before = self.read(m, 'highscore_entries', 70)
        m.call('initialize_score')
        self.assertEqual(self.read(m, 'score_digits', 6), [0]*6)
        self.assertEqual(self.read(m, 'highscore_entries', 70), before)
        self.assertEqual(m.get('highscore_count'), 2)
        self.assertEqual(m.get('highscore_dirty'), 1)

    def test_name_reset_and_all_blank_confirmation(self):
        m = self.new()
        self.put(m, 'highscore_name', [1]*NAME_LENGTH)
        m.call('highscore_name_reset')
        self.assertEqual(self.name(m), [32]*NAME_LENGTH)
        self.assertEqual(m.get('highscore_name_length'), 0)
        self.assertFalse(self.key(m, 13))
        for _ in range(NAME_LENGTH):
            self.assertFalse(self.key(m, 32))
        self.assertFalse(self.key(m, 13))
        self.assertEqual(self.name(m), [32]*NAME_LENGTH)

    def test_name_accepts_eight_characters_and_converts_petscii_letters(self):
        m = self.new()
        m.call('highscore_name_reset')
        for char in 'AZ 019MT':
            self.assertFalse(self.key(m, ord(char)))
        self.assertEqual(self.name(m), screen_codes('AZ 019MT'))
        self.assertEqual(m.get('highscore_name_length'), 8)
        self.assertFalse(self.key(m, ord('X')))
        self.assertEqual(self.name(m), screen_codes('AZ 019MT'))
        self.assertEqual(m.get('highscore_name_length'), 8)
        self.assertTrue(self.key(m, 13))

    def test_delete_bounds_and_retype(self):
        m = self.new()
        m.call('highscore_name_reset')
        self.assertFalse(self.key(m, 20))
        self.assertEqual(m.get('highscore_name_length'), 0)
        for char in 'ALICE123':
            self.key(m, ord(char))
        self.assertFalse(self.key(m, 20))
        self.assertEqual(m.get('highscore_name_length'), 7)
        self.assertEqual(self.name(m), screen_codes('ALICE12 '))
        self.key(m, ord('9'))
        self.assertEqual(self.name(m), screen_codes('ALICE129'))
        for _ in range(9):
            self.assertFalse(self.key(m, 20))
        self.assertEqual(m.get('highscore_name_length'), 0)
        self.assertEqual(self.name(m), [32]*8)
        self.assertFalse(self.key(m, 13))

    def test_lowercase_name_is_normalized_to_uppercase_screen_codes(self):
        m = self.new()
        m.call('highscore_name_reset')
        for char in 'aZ09 mT':
            self.assertFalse(self.key(m, ord(char)))
        self.assertEqual(self.name(m), screen_codes('AZ09 MT '))
        self.assertEqual(m.get('highscore_name_length'), 7)
        self.assertTrue(self.key(m, 13))

    def test_disallowed_name_keys_do_not_change_input(self):
        m = self.new()
        m.call('highscore_name_reset')
        self.key(m, ord('A'))
        for key in [0, 1, 3, 17, 29, 31, 33, 47, 58, 64, 91, 127, 145, 157, 255]:
            with self.subTest(key=key):
                before = self.name(m)
                self.assertFalse(self.key(m, key))
                self.assertEqual(self.name(m), before)
                self.assertEqual(m.get('highscore_name_length'), 1)

    def test_confirmed_name_is_stored_with_score(self):
        m = self.new()
        self.assertEqual(self.rank(m, 123456), 0)
        m.call('highscore_name_reset')
        for char in 'PLAYER 1':
            self.key(m, ord(char))
        self.assertTrue(self.key(m, 13))
        m.call('highscore_insert')
        self.assertEqual(self.table(m), [record('PLAYER 1', 123456)])

    def test_table_render_uses_text_page_both_screens_and_no_sprite(self):
        m = self.new()
        self.set_table(m, [('ALICE123', 123456), ('BOB 4567', 999)])
        self.put(m, 'score_digits', score_digits(123456))
        m.set('highscore_rank', 0)
        m.mem[0xd015] = 0xff
        m.call('show_highscore_table')
        self.assertEqual(m.mem[0xd015], 0)
        self.assertEqual(m.mem[0x0400:0x07e8], m.mem[0x0c00:0x0fe8])
        self.assertEqual(m.mem[0x2800:0x2a00], m.mem[0x4800:0x4a00])
        self.assertEqual(m.mem[0xd011] & 16, 16)
        screen = bytes(m.mem[0x0400:0x07e8])
        for text in ['GAME OVER', 'ALICE123', 'BOB 4567', '123456', '000999']:
            self.assertIn(bytes(screen_codes(text)), screen, text)

    def test_repeated_game_over_render_never_registers_score_again(self):
        m = self.new()
        self.insert(m, 'ALICE', 128)
        self.insert(m, 'BOB', 96)
        self.insert(m, 'YOU', 80)
        before = self.read(m, 'highscore_entries', 70)
        for dirty in [0, 1]:
            m.set('highscore_dirty', dirty)
            for _ in range(3):
                m.call('show_game_over_screen')
                self.assertEqual(self.read(m, 'highscore_entries', 70), before)
                self.assertEqual(m.get('highscore_count'), 3)
                self.assertEqual(m.get('highscore_dirty'), dirty)
                self.assertEqual(m.mem[0xd015], 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    ARGS = parser.parse_args()
    unittest.main(argv=['test_highscores'], verbosity=2)
