#!/usr/bin/env python3
"""分析 PCM WAV，并输出适合 SID 重编曲的节拍、调性和音高草稿。"""

from __future__ import annotations

import argparse
import json
import math
import wave
from pathlib import Path

import numpy as np


NOTE_NAMES = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
MAJOR_PROFILE = np.array([6.35, 2.23, 3.48, 2.33, 4.38, 4.09,
                          2.52, 5.19, 2.39, 3.66, 2.29, 2.88])
MINOR_PROFILE = np.array([6.33, 2.68, 3.52, 5.38, 2.60, 3.53,
                          2.54, 4.75, 3.98, 2.69, 3.34, 3.17])


def read_wav(path: Path, target_rate: int = 11025) -> tuple[np.ndarray, int, dict[str, int]]:
    with wave.open(str(path), "rb") as wav_file:
        channels = wav_file.getnchannels()
        sample_width = wav_file.getsampwidth()
        source_rate = wav_file.getframerate()
        frame_count = wav_file.getnframes()
        raw = wav_file.readframes(frame_count)

    if sample_width != 2:
        raise ValueError(f"仅支持 16 位 PCM WAV，当前样本宽度为 {sample_width} 字节")

    samples = np.frombuffer(raw, dtype="<i2").astype(np.float64)
    samples = samples.reshape(-1, channels).mean(axis=1) / 32768.0
    if source_rate != target_rate:
        duration = len(samples) / source_rate
        target_count = int(round(duration * target_rate))
        source_positions = np.linspace(0.0, len(samples) - 1, target_count)
        samples = np.interp(source_positions, np.arange(len(samples)), samples)

    samples -= np.mean(samples)
    peak = np.max(np.abs(samples))
    if peak:
        samples /= peak

    metadata = {
        "channels": channels,
        "sample_width": sample_width,
        "source_rate": source_rate,
        "source_frames": frame_count,
    }
    return samples, target_rate, metadata


def stft(samples: np.ndarray, fft_size: int = 4096, hop: int = 256) -> tuple[np.ndarray, np.ndarray]:
    frame_count = 1 + max(0, (len(samples) - fft_size) // hop)
    shape = (frame_count, fft_size)
    strides = (samples.strides[0] * hop, samples.strides[0])
    frames = np.lib.stride_tricks.as_strided(samples, shape=shape, strides=strides)
    windowed = frames * np.hanning(fft_size)
    spectrum = np.abs(np.fft.rfft(windowed, axis=1))
    rms = np.sqrt(np.mean(frames * frames, axis=1))
    return spectrum, rms


def estimate_tempo(
    spectrum: np.ndarray, rate: int, hop: int
) -> tuple[float, float, np.ndarray, list[float]]:
    log_spectrum = np.log1p(20.0 * spectrum)
    flux = np.maximum(0.0, np.diff(log_spectrum, axis=0)).sum(axis=1)
    flux = np.concatenate(([0.0], flux))
    smooth_width = max(1, round((rate / hop) * 0.08))
    smooth = np.convolve(flux, np.ones(smooth_width) / smooth_width, mode="same")
    onset = np.maximum(0.0, flux - smooth)
    onset -= np.mean(onset)

    envelope_rate = rate / hop
    correlation = np.correlate(onset, onset, mode="full")[len(onset) - 1:]
    min_lag = max(1, int(envelope_rate * 60.0 / 190.0))
    max_lag = min(len(correlation) - 1, int(envelope_rate * 60.0 / 65.0))
    lags = np.arange(min_lag, max_lag + 1)
    scores = correlation[lags] / np.sqrt(lags)
    ranked = np.argsort(scores)[::-1]
    tempo_candidates: list[float] = []
    for candidate_index in ranked:
        candidate = 60.0 * envelope_rate / int(lags[candidate_index])
        if all(abs(candidate - previous) > 3.0 for previous in tempo_candidates):
            tempo_candidates.append(float(candidate))
        if len(tempo_candidates) == 5:
            break
    lag = int(lags[ranked[0]])
    bpm = 60.0 * envelope_rate / lag

    period = envelope_rate * 60.0 / bpm
    phases = np.arange(max(1, int(round(period))))
    phase_scores = np.array([onset[p::max(1, int(round(period)))].sum() for p in phases])
    phase = float(phases[np.argmax(phase_scores)] / envelope_rate)
    return bpm, phase, onset, tempo_candidates


def chroma_energy(spectrum: np.ndarray, rate: int, fft_size: int) -> np.ndarray:
    frequencies = np.fft.rfftfreq(fft_size, 1.0 / rate)
    valid = (frequencies >= 55.0) & (frequencies <= 2200.0)
    midi = np.rint(69.0 + 12.0 * np.log2(frequencies[valid] / 440.0)).astype(int)
    chroma = np.zeros((spectrum.shape[0], 12), dtype=np.float64)
    weighted = np.log1p(spectrum[:, valid])
    for pitch_class in range(12):
        chroma[:, pitch_class] = weighted[:, midi % 12 == pitch_class].sum(axis=1)
    return chroma


def estimate_key(chroma: np.ndarray) -> tuple[str, str, float, list[dict[str, float | str]]]:
    energy = chroma.sum(axis=0)
    energy = (energy - np.mean(energy)) / (np.std(energy) + 1e-9)
    candidates: list[tuple[float, int, str]] = []
    for root in range(12):
        major = np.roll(MAJOR_PROFILE, root)
        minor = np.roll(MINOR_PROFILE, root)
        candidates.append((float(np.corrcoef(energy, major)[0, 1]), root, "major"))
        candidates.append((float(np.corrcoef(energy, minor)[0, 1]), root, "minor"))
    ranked = sorted(candidates, reverse=True)
    score, root, mode = ranked[0]
    top_candidates = [
        {"key": f"{NOTE_NAMES[candidate_root]} {candidate_mode}", "score": round(candidate_score, 4)}
        for candidate_score, candidate_root, candidate_mode in ranked[:5]
    ]
    return NOTE_NAMES[root], mode, score, top_candidates


def note_name(midi_note: int) -> str:
    return f"{NOTE_NAMES[midi_note % 12]}{midi_note // 12 - 1}"


def smooth_note_path(step_energy: np.ndarray, candidates: np.ndarray, center: int) -> np.ndarray:
    """用动态规划在混音频谱中寻找连续、可演奏的单声部音高线。"""
    scores = np.log1p(step_energy[:, candidates - 24])
    scores = (scores - scores.mean(axis=1, keepdims=True)) / (
        scores.std(axis=1, keepdims=True) + 1e-9
    )
    scores -= 0.012 * np.abs(candidates - center)[None, :]

    transition = 0.13 * np.abs(candidates[:, None] - candidates[None, :])
    transition += 0.28 * np.maximum(0, np.abs(candidates[:, None] - candidates[None, :]) - 7)
    accumulated = scores[0].copy()
    backtrack = np.zeros((scores.shape[0], len(candidates)), dtype=np.int16)
    for step in range(1, scores.shape[0]):
        alternatives = accumulated[:, None] - transition
        previous = np.argmax(alternatives, axis=0)
        accumulated = alternatives[previous, np.arange(len(candidates))] + scores[step]
        backtrack[step] = previous

    path = np.empty(scores.shape[0], dtype=np.int16)
    path[-1] = int(np.argmax(accumulated))
    for step in range(scores.shape[0] - 1, 0, -1):
        path[step - 1] = backtrack[step, path[step]]
    return candidates[path]


def extract_note_grid(
    spectrum: np.ndarray,
    rate: int,
    fft_size: int,
    hop: int,
    bpm: float,
    phase_seconds: float,
    duration: float,
) -> dict[str, list[str]]:
    frequencies = np.fft.rfftfreq(fft_size, 1.0 / rate)
    midi_frequencies = 440.0 * (2.0 ** ((np.arange(24, 97) - 69.0) / 12.0))
    harmonic_energy = np.zeros((spectrum.shape[0], len(midi_frequencies)), dtype=np.float64)
    for note_index, frequency in enumerate(midi_frequencies):
        for harmonic, weight in ((1, 1.0), (2, 0.45), (3, 0.25)):
            target = frequency * harmonic
            if target >= rate / 2:
                continue
            bin_index = int(round(target * fft_size / rate))
            low = max(0, bin_index - 1)
            high = min(spectrum.shape[1], bin_index + 2)
            harmonic_energy[:, note_index] += weight * spectrum[:, low:high].max(axis=1)

    step_seconds = 60.0 / bpm / 2.0
    start = max(0.0, phase_seconds)
    available_duration = min(duration, spectrum.shape[0] * hop / rate)
    steps = max(1, int(math.floor((available_duration - start) / step_seconds)))
    step_energy = np.zeros((steps, harmonic_energy.shape[1]), dtype=np.float64)

    for step in range(steps):
        begin = start + step * step_seconds
        end = begin + step_seconds
        frame_begin = max(0, int(begin * rate / hop))
        frame_end = min(spectrum.shape[0], max(frame_begin + 1, int(end * rate / hop)))
        step_energy[step] = np.median(harmonic_energy[frame_begin:frame_end], axis=0)

    lead_path = smooth_note_path(step_energy, np.arange(48, 85), center=67)
    bass_path = smooth_note_path(step_energy, np.arange(28, 53), center=40)

    bar_roots: list[str] = []
    steps_per_bar = 8
    for bar_start in range(0, len(bass_path), steps_per_bar):
        bar_end = min(len(bass_path), bar_start + steps_per_bar)
        pitch_classes = np.zeros(12)
        for step in range(bar_start, bar_end):
            for note in range(28, 53):
                pitch_classes[note % 12] += step_energy[step, note - 24]
        root_class = int(np.argmax(pitch_classes))
        root_note = 36 + root_class
        if root_note > 47:
            root_note -= 12
        bar_roots.append(note_name(root_note))

    return {
        "lead_eighth_notes": [note_name(int(note)) for note in lead_path],
        "bass_eighth_notes": [note_name(int(note)) for note in bass_path],
        "estimated_bar_roots": bar_roots,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("wav", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()

    fft_size = 4096
    hop = 256
    samples, rate, metadata = read_wav(args.wav)
    spectrum, rms = stft(samples, fft_size=fft_size, hop=hop)
    bpm, phase, _, tempo_candidates = estimate_tempo(spectrum, rate, hop)
    chroma = chroma_energy(spectrum, rate, fft_size)
    key, mode, key_score, key_candidates = estimate_key(chroma)
    duration = len(samples) / rate
    note_grid = extract_note_grid(spectrum, rate, fft_size, hop, bpm, phase, duration)

    result = {
        "file": str(args.wav),
        "duration_seconds": round(duration, 3),
        "analysis_rate": rate,
        "source": metadata,
        "rms_mean": round(float(np.mean(rms)), 6),
        "rms_peak": round(float(np.max(rms)), 6),
        "estimated_bpm": round(bpm, 3),
        "tempo_candidates": [round(candidate, 3) for candidate in tempo_candidates],
        "estimated_first_beat_seconds": round(phase, 3),
        "estimated_key": f"{key} {mode}",
        "key_correlation": round(key_score, 4),
        "key_candidates": key_candidates,
        "pitch_class_energy": {
            NOTE_NAMES[index]: round(float(value), 4)
            for index, value in enumerate(chroma.sum(axis=0) / np.sum(chroma))
        },
        **note_grid,
    }
    output = json.dumps(result, ensure_ascii=False, indent=2)
    print(output)
    if args.json:
        args.json.write_text(output + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
