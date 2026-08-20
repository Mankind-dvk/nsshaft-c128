#!/usr/bin/env python3
"""将 WAV 渲染为带音高和小节网格的频谱图，辅助人工 SID 重编曲。"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from analyze_music import NOTE_NAMES, read_wav, stft


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("wav", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--bpm", type=float, default=130.0)
    parser.add_argument("--offset", type=float, default=0.325)
    args = parser.parse_args()

    fft_size = 4096
    hop = 256
    samples, rate, _ = read_wav(args.wav)
    spectrum, _ = stft(samples, fft_size=fft_size, hop=hop)
    frequencies = np.fft.rfftfreq(fft_size, 1.0 / rate)

    midi_notes = np.arange(24, 97)
    note_frequencies = 440.0 * 2.0 ** ((midi_notes - 69.0) / 12.0)
    piano_roll = np.empty((len(midi_notes), spectrum.shape[0]), dtype=np.float64)
    for index, frequency in enumerate(note_frequencies):
        center = int(round(frequency * fft_size / rate))
        low = max(0, center - 1)
        high = min(spectrum.shape[1], center + 2)
        piano_roll[index] = spectrum[:, low:high].max(axis=1)

    piano_roll = np.log1p(piano_roll)
    floor, ceiling = np.percentile(piano_roll, (35.0, 99.5))
    piano_roll = np.clip((piano_roll - floor) / (ceiling - floor + 1e-9), 0.0, 1.0)

    plot_width = 2200
    row_height = 10
    label_width = 70
    plot_height = len(midi_notes) * row_height
    image = Image.new("RGB", (label_width + plot_width, plot_height), (10, 10, 14))
    pixels = np.empty((plot_height, plot_width, 3), dtype=np.uint8)
    source_x = np.linspace(0, piano_roll.shape[1] - 1, plot_width).astype(int)
    expanded = np.repeat(piano_roll[:, source_x], row_height, axis=0)[::-1]
    pixels[:, :, 0] = np.clip(expanded * 255, 0, 255).astype(np.uint8)
    pixels[:, :, 1] = np.clip(expanded ** 1.3 * 210, 0, 255).astype(np.uint8)
    pixels[:, :, 2] = np.clip(expanded ** 2.0 * 90, 0, 255).astype(np.uint8)
    image.paste(Image.fromarray(pixels, "RGB"), (label_width, 0))

    draw = ImageDraw.Draw(image)
    font = ImageFont.load_default()
    duration = len(samples) / rate
    beat_seconds = 60.0 / args.bpm

    for midi_note in midi_notes:
        y = (96 - midi_note) * row_height
        if midi_note % 12 == 0:
            draw.line((label_width, y, label_width + plot_width, y), fill=(55, 55, 70))
            label = f"{NOTE_NAMES[midi_note % 12]}{midi_note // 12 - 1}"
            draw.text((4, y + 1), label, font=font, fill=(220, 220, 230))

    beat = 0
    time = args.offset
    while time < duration:
        x = label_width + int(time / duration * plot_width)
        if beat % 8 == 0:
            color = (80, 200, 255)
        elif beat % 2 == 0:
            color = (80, 100, 130)
        else:
            color = (45, 55, 70)
        draw.line((x, 0, x, plot_height), fill=color)
        beat += 1
        time += beat_seconds

    args.output.parent.mkdir(parents=True, exist_ok=True)
    image.save(args.output)


if __name__ == "__main__":
    main()
