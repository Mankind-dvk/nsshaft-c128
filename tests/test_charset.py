"""Execute compiled 6502 routines; no VIC raster, SID, or MMU emulation.

Requires py65==1.2.0; Pillow is optional (only for --render-directory).
Run build.ps1 first. --baseline-dir accepts the pre-reorder PRG and label file
from commit 567c2e3. HUD font pixels are normalized to the teacher's font before
old/new scene comparisons. The intentional frame-texture and fixed-ceiling changes
and normal-platform texture are normalized in legacy pixel comparisons;
test_brick_platforms verifies the new texture without normalization. Changed
spike cells are masked only in legacy comparisons; test_spike_material checks
their exact white/green pixels and vertical motion independently.
"""
import argparse
from pathlib import Path
import unittest

from py65.devices.mpu6502 import MPU

ROOT = Path(__file__).resolve().parents[1]
ARGS = None
OLD_GLYPH_AT_NEW_SLOT = [
    0, 1, 2, 36, 47, 19, 21, 60,
    31, 32, 33, 34, 43, 46, 48, 49, 61, 62, 20, 30,
    35, 3, 37, 38, 39, 40, 41, 42, 44, 45, 50, 51,
    52, 53, 54, 55, 56, 57, 58, 59,
    4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 22, 15, 14, 16, 17, 18,
    23, 26, 24, 28, 27, 29, 25, 63,
]
HUD_SOURCE_IDS = list(range(48, 58)) + [8, 15, 16, 20, 24, 58]
OLD_TO_NEW = {old: new for new, old in enumerate(OLD_GLYPH_AT_NEW_SLOT)}
ECM_TO_MIXED = {code: code for code in range(64)}
ECM_TO_MIXED.update({
    0x41: 1, 0x82: 2, 0xc1: 56, 0x48: 8, 0x49: 9,
    0x8a: 10, 0x8b: 11, 0xc8: 57, 0xc9: 58,
    0x87: 59, 0x90: 60, 0x91: 61, 0xc7: 7, 0xd0: 16, 0xd1: 17,
})


def remap(code):
    return ECM_TO_MIXED[(code & 0xc0) | OLD_TO_NEW[code & 0x3f]]


class Machine:
    def __init__(self, directory):
        self.cpu = MPU()
        self.mem = self.cpu.memory
        raw = (directory / 'nsshaft-c128.prg').read_bytes()
        start = int.from_bytes(raw[:2], 'little')
        self.mem[start:start+len(raw)-2] = raw[2:]
        self.labels = {}
        for line in (directory / 'nsshaft-c128.lbl').read_text().splitlines():
            _, address, name = line.split()
            if not name.startswith('.@'):
                self.labels[name.lstrip('.')] = int(address, 16)
        self.mem[0xd021:0xd025] = [0, 1, 2, 12]
        self.mem[0xd018] = 0x1a
        self.mem[0xd011] = 0x5b
        self.mem[0xd016] = 8

    def get(self, name):
        return self.mem[self.labels[name]]

    def set(self, name, value):
        self.mem[self.labels[name]] = value

    def call(self, name, a=0, x=0, y=0):
        cpu = self.cpu
        cpu.a, cpu.x, cpu.y = a, x, y
        cpu.p = cpu.UNUSED | cpu.INTERRUPT
        cpu.sp = 0xff
        cpu.stPushWord(0x01ff)
        cpu.pc = self.labels[name]
        for _ in range(150000):
            if cpu.pc == 0x0200:
                if cpu.sp != 0xff:
                    raise AssertionError(f'Unbalanced stack: {name}')
                return cpu.a
            cpu.step()
        raise AssertionError(f'Routine failed to return: {name}, PC={cpu.pc:04x}')

    def pixels(self):
        """Decode settled ECM or mixed hires/multicolor, without sprites."""
        screen = (self.mem[0xd018] >> 4) * 1024
        charset = ((self.mem[0xd018] >> 1) & 7) * 2048
        pixels = bytearray(320*200)
        ecm = bool(self.mem[0xd011] & 64)
        mcm = bool(self.mem[0xd016] & 16) and not ecm
        for row in range(25):
            for col in range(40):
                cell = row*40+col
                code = self.mem[screen+cell]
                foreground = self.mem[0xd800+cell] & 15
                background = self.mem[0xd021+(code >> 6 if ecm else 0)] & 15
                glyph = code & 63 if ecm else code
                colors = [background, self.mem[0xd022] & 15,
                          self.mem[0xd023] & 15, foreground & 7]
                for gy in range(8):
                    bits = self.mem[charset+glyph*8+gy]
                    for gx in range(8):
                        if mcm and foreground & 8:
                            color = colors[(bits >> (6-2*(gx//2))) & 3]
                        else:
                            color = foreground if bits & (128 >> gx) else background
                        pixels[(row*8+gy)*320+col*8+gx] = color
        return bytes(pixels)

    def moving_region_pixels(self):
        # Compare rows 3..24, masking only the fixed frame cells whose texture
        # intentionally changed. test_ceiling verifies their exact new pixels.
        pixels = bytearray(self.pixels())
        # Preserve the silhouette while ignoring the intentional normal texture.
        if 'update_brick_and_fade_fragments' in self.labels:
            screen = (self.mem[0xd018] >> 4)*1024
            for cell in range(1000):
                if self.mem[screen+cell] in [1, 8, 9]:
                    row, col = divmod(cell, 40)
                    for gy in range(8):
                        offset = (row*8+gy)*320+col*8
                        pixels[offset:offset+8] = bytes(1 if p else 0 for p in pixels[offset:offset+8])
        screen = (self.mem[0xd018] >> 4)*1024
        spike_codes = [2, 10, 11] if 'update_brick_and_fade_fragments' in self.labels else [0x82, 0xa1, 0xa2]
        for cell in range(1000):
            if self.mem[screen+cell] in spike_codes:
                row, col = divmod(cell, 40)
                for gy in range(8):
                    offset = (row*8+gy)*320+col*8
                    pixels[offset:offset+8] = bytes(8)
        for row in range(1, 24):
            columns = range(1, 39) if row in [1, 23] else [1, 29, 38]
            for col in columns:
                for gy in range(8):
                    offset = (row*8+gy)*320+col*8
                    pixels[offset:offset+8] = bytes([1]*8)
        return bytes(pixels[3*8*320:])

    def fixed_glyphs(self):
        return bytes(value for glyph in list(range(20, 57))+[59, 62, 63]
                     for value in self.mem[0x2800+glyph*8:0x2808+glyph*8])

    def setup_game(self):
        if 'load_game_charset' in self.labels:
            self.call('load_game_charset')
        for name in ['clear_screens_and_colors', 'initialize_fade_platforms',
                     'initialize_spring_platforms', 'initialize_conveyor_platforms',
                     'initialize_score', 'initialize_health', 'initialize_ui',
                     'initialize_hud']:
            self.call(name)
        self.set('fine_scroll', 7)
        self.set('active_screen', 0)
        self.set('player_on_platform', 0)
        self.set('rows_until_platform', 200)
        self.set('potx_raw', 128)
        self.call('update_potx_display')
        for kind in range(6):
            self.set('platform_type', kind)
            self.set('platform_start', 3+kind)
            self.set('platform_width', 7+kind)
            address = 0x0400+(4+kind*3)*40
            self.mem[0xfb:0xfd] = [address & 255, address >> 8]
            self.call('draw_platform')
        self.call('prepare_screen_b_from_a')


class CharsetTests(unittest.TestCase):
    def new(self):
        return Machine(ARGS.build_dir)

    def pair(self):
        if ARGS.baseline_dir is None:
            self.skipTest('Use --baseline-dir for pre-reorder comparisons')
        old, new = Machine(ARGS.baseline_dir), self.new()
        # Normalize only the intentional font change, not platform graphics.
        for slot, source_id in enumerate(HUD_SOURCE_IDS, 40):
            old_id = OLD_GLYPH_AT_NEW_SLOT[slot]
            old.mem[0x2800+old_id*8:0x2808+old_id*8] = new.mem[0x4800+source_id*8:0x4808+source_id*8]
        return old, new

    def save_render(self, name, machine):
        if ARGS.render_directory is None:
            return
        from PIL import Image
        palette = [0x000000, 0xffffff, 0x984b43, 0x79c1c8, 0x9b51a5,
                   0x68ae5c, 0x494b9b, 0xddd778, 0x9b6739, 0x675000,
                   0xc37b75, 0x606060, 0x8b8b8b, 0xa2df96, 0x898be0, 0xb5b5b5]
        rgb = bytes(channel for pixel in machine.pixels()
                    for channel in palette[pixel].to_bytes(3, 'big'))
        ARGS.render_directory.mkdir(parents=True, exist_ok=True)
        Image.frombytes('RGB', (320, 200), rgb).resize((960, 600), Image.Resampling.NEAREST).save(ARGS.render_directory / (name+'.png'))

    def test_source_and_output_layout(self):
        m = self.new()
        self.assertEqual(len(set(OLD_GLYPH_AT_NEW_SLOT)), 64)
        for name, address in [('charset_begin', 0x2800), ('charset_end', 0x2a20),
                              ('graphics_charset', 0x4000), ('graphics_charset_end', 0x4800),
                              ('text_charset', 0x4800), ('text_charset_end', 0x5000)]:
            self.assertEqual(m.labels[name], address)
        self.assertEqual(m.mem[0x4000+22*8:0x4000+56*8], [0]*(34*8))
        self.assertEqual(m.mem[0x4000+62*8:0x4800], [0]*(2048-62*8))
        self.assertEqual(bytes(m.mem[0x4800:0x5000]), (ROOT/'assets/fonts/c64-upper.bin').read_bytes())
        self.assertEqual(m.mem[0x2800:0x2a20], [0]*544)
        sources = m.mem[0x4000:0x5000].copy()
        m.call('load_game_charset')
        self.assertEqual(m.mem[0x2800:0x2800+22*8], sources[:22*8])
        self.assertEqual(m.mem[0x2800+22*8:0x2800+40*8], [0]*(18*8))
        for slot, source in enumerate(HUD_SOURCE_IDS, 40):
            self.assertEqual(m.mem[0x2800+slot*8:0x2808+slot*8],
                             m.mem[0x4800+source*8:0x4808+source*8])
        self.assertEqual(m.mem[0x2800+56*8:0x2800+62*8], sources[56*8:62*8])
        for slot, source in enumerate([19, 3, 18, 5], 62):
            self.assertEqual(m.mem[0x2800+slot*8:0x2808+slot*8],
                             m.mem[0x4800+source*8:0x4808+source*8])
        self.assertEqual(m.mem[0x4000:0x5000], sources)
        self.assertEqual(m.mem[0xd011] & 16, 0)

    def test_copy_ranges_and_page_carries(self):
        for count in [0, 1, 2, 31, 32, 64]:
            m = self.new()
            source, destination = 0x60fc, 0x65fc
            length = count*8
            m.mem[source:source+512] = [(i*37+11) & 255 for i in range(512)]
            m.mem[destination-1:destination+513] = [0xa5]*514
            m.mem[0xfb:0xff] = [source & 255, source >> 8, destination & 255, destination >> 8]
            m.call('copy_charset_glyphs', x=count)
            self.assertEqual(m.mem[destination:destination+length], m.mem[source:source+length])
            self.assertEqual(m.mem[destination-1], 0xa5)
            self.assertEqual(m.mem[destination+length:destination+513], [0xa5]*(513-length))
            self.assertEqual(m.mem[0xfb] | m.mem[0xfc] << 8, source+length)
            self.assertEqual(m.mem[0xfd] | m.mem[0xfe] << 8, destination+length)

    def test_text_page_and_game_reentry(self):
        m = self.new()
        sources = m.mem[0x4000:0x5000].copy()
        for _ in range(3):
            m.call('show_start_screen')
            self.assertEqual(m.mem[0x2800:0x2a00], m.mem[0x4800:0x4a00])
            self.assertEqual(m.mem[0x0400], 32)
            self.assertEqual(m.mem[0x0c00], 32)
            m.call('draw_centering_prompt')
            self.save_render('centering', m)
            m.setup_game()
            fixed = m.fixed_glyphs()
            for _ in range(9):
                m.call('scroll_one_pixel_up')
            self.assertEqual(m.fixed_glyphs(), fixed)
            m.call('show_game_over_screen')
            self.assertEqual(m.mem[0x2800:0x2a00], m.mem[0x4800:0x4a00])
            self.assertEqual(m.mem[0x4000:0x5000], sources)
            self.assertEqual(m.mem[0xd011] & 16, 16)

    def test_full_alphabet_symbols_and_menu_calls(self):
        m = self.new()
        m.call('show_start_screen')
        for routine, offset, message in [
            (None, 8*40+16, 'NS-SHAFT'),
            ('draw_centering_prompt', 14*40+2, 'PRESS FIRE WHEN CHARACTER FACES YOU'),
            ('show_game_over_screen', 3*40+15, 'GAME OVER')]:
            if routine:
                m.call(routine)
            expected = [ord(c) & 63 for c in message]
            for screen in [0x0400, 0x0c00]:
                self.assertEqual(m.mem[screen+offset:screen+offset+len(expected)], expected)
        m.call('show_start_screen')
        for code in range(64):
            offset = (code//8+2)*40+code%8+2
            m.mem[0x0400+offset] = code
            m.mem[0xd800+offset] = 1
            if code != 32:
                self.assertNotEqual(m.mem[0x2800+code*8:0x2808+code*8], [0]*8, code)
        self.save_render('text-page', m)

    def test_fade_mapping_exhaustive(self):
        m = self.new()
        inactive, cracked, broken = [56, 57, 58], [7, 16, 17], [59, 60, 61]
        for mode in range(3):
            m.set('fade_effect_mode', mode)
            for stage in [0, 1]:
                m.set('fade_stage', stage)
                target = broken if stage else cracked
                for char in range(256):
                    result = m.call('transform_fade_character', a=char)
                    expected = char
                    if mode == 0 and char in inactive:
                        expected = target[inactive.index(char)]
                    elif mode != 0 and char in cracked+broken:
                        index = (cracked+broken).index(char) % 3
                        expected = target[index] if mode == 1 else 0
                    self.assertEqual(result, expected, (mode, stage, char))

    def test_old_new_transform_equivalence(self):
        old, new = self.pair()
        for routine, state, modes in [('transform_fade_character', 'fade_effect_mode',3),
                                      ('transform_spring_character','spring_effect_mode',2)]:
            for mode in range(modes):
                old.set(state,mode)
                new.set(state,mode)
                for prefix in [0x80,0xc0]:
                    old.set('fade_color_prefix',prefix)
                    new.set('fade_stage',1 if prefix == 0x80 else 0)
                    canonical = [0, 0x41, 0x82, 3, 4, 5, 6, 0xc1, 0x48, 0x49,
                                 0x8a, 0x8b, 12, 13, 14, 15, 18, 19, 20,
                                 0xc8, 0xc9, 0x87, 0x90, 0x91, 0xc7, 0xd0, 0xd1]
                    for code in canonical + list(range(40, 56)):
                        char = (code & 0xc0) | OLD_GLYPH_AT_NEW_SLOT[code & 63]
                        expected=remap(old.call(routine,a=char))
                        self.assertEqual(new.call(routine,a=remap(char)),expected,(routine,mode,char))

    def test_original_player_sprite_assets_unchanged(self):
        old,new=self.pair()
        for start,end in [(0x3000,0x3100),(0x3a00,0x3cc0)]:
            self.assertEqual(old.mem[start:end],new.mem[start:end])

    def test_scroll_pixels_and_collision_equivalence(self):
        old,new=self.pair()
        old.setup_game()
        new.setup_game()
        permanent=new.fixed_glyphs()
        for step in range(97):
            self.assertEqual(old.moving_region_pixels(),new.moving_region_pixels(),f'scroll step {step}')
            self.assertEqual(new.fixed_glyphs(),permanent)
            if step in [0,1,4,7,8]:
                self.save_render(f'scroll-{step:02d}',new)
            if step <= 8:
                for row in range(3,23):
                    for delta in [-1,0,1,2,3]:
                        for x in [40,88,136,232]:
                            for m in [old,new]:
                                m.set('player_x_low',x)
                                m.set('player_x_high',0)
                                m.set('player_y',(43+m.get('fine_scroll')+row*8+delta-21)&255)
                            self.assertEqual(old.call('find_platform_below_player'),new.call('find_platform_below_player'),(step,row,delta,x))
            old.call('scroll_one_pixel_up')
            new.call('scroll_one_pixel_up')

    def test_active_fade_visual_lifecycle(self):
        old,new=self.pair()
        old.setup_game()
        new.setup_game()
        for m in [old,new]:
            m.set('collision_row',10)
            m.call('activate_fade_platform')
        for frame in range(101):
            self.assertEqual(old.moving_region_pixels(),new.moving_region_pixels(),f'fade frame {frame}')
            for m in [old,new]:
                m.call('update_fade_platform')
                if frame%2==0:
                    m.call('scroll_one_pixel_up')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir',type=Path,default=ROOT/'build')
    parser.add_argument('--baseline-dir',type=Path)
    parser.add_argument('--render-directory',type=Path)
    ARGS=parser.parse_args()
    unittest.main(argv=['test_charset'],verbosity=2)
