#!/usr/bin/env python3
"""Remove avoidable dispatch and integer division in pinned VCO waveforms.

Keep the pinned Phasor phase values and existing floating-point waveforms.
Do not apply the qualified call to pointers or other polymorphic receivers.
"""
import argparse
from pathlib import Path


def prepare_phase_cycles(source):
    root = source / "plugins/BogaudioModules/src/dsp"
    header = (root / "oscillator.hpp").read_text()
    for declaration in ("typedef uint64_t phase_t;",
                        "static constexpr phase_t cyclePhase = UINT32_MAX;"):
        if header.count(declaration) != 1:
            raise SystemExit("Pinned unsigned phase representation mismatch")
    path = root / "oscillator.cpp"
    text = path.read_text()
    signature = "float BandLimitedSquareOscillator::nextForPhase(phase_t phase) {"
    old = signature + "\n\tphase_t cycle = phase / cyclePhase;"
    new = signature + '''
	// phase = high * cyclePhase + (low + high), with cyclePhase = 2^32 - 1.
	// The correction is 0, 1 or 2; preserve the full unsigned 64-bit domain.
	phase_t cycle = phase >> 32;
	const phase_t sum = static_cast<uint32_t>(phase) + cycle;
	if (sum >= cyclePhase) {
		cycle += 1 + (sum == 2 * cyclePhase);
	}'''
    if new in text:
        if text.count(new) != 1 or old in text:
            raise SystemExit("Incomplete square phase-cycle preparation")
        print("Square phase-cycle preparation already present")
        return
    if text.count(signature) != 1 or text.count(old) != 1:
        raise SystemExit("Pinned square phase-cycle seam mismatch")
    path.write_text(text.replace(old, new))
    print("Prepared exact square phase-cycle quotient")


def prepare(source):
    root = source / "plugins/BogaudioModules/src"
    path = root / "vco_base.cpp"
    text = path.read_text()
    oscillator = (root / "dsp/oscillator.cpp").read_text()
    signature = "float Phasor::nextFromPhasor(const Phasor& phasor, phase_delta_t offset) {"
    if oscillator.count(signature) != 1:
        raise SystemExit("Pinned Phasor phase seam mismatch")
    start = oscillator.index(signature) + len(signature)
    stop = oscillator.index("\n}", start)
    body = oscillator[start:stop]
    if body != '''
	offset += phasor._phase;
	if (_samplePhase > 0) {
		offset -= offset % _samplePhase;
	}
	return nextForPhase(offset);''':
        raise SystemExit("Pinned Phasor phase arithmetic mismatch")
    body = body.replace("_samplePhase", "wave._samplePhase")
    body = body.replace("nextForPhase(offset)", "wave.Wave::nextForPhase(offset)")
    helper = '''namespace {
// These are value members, so their dynamic types equal their declared types.
template<class Wave>
float nextOwnedWave(Wave& wave, const Phasor& phasor, Phasor::phase_delta_t offset) {'''
    helper += body + "\n}\n}\n\n"
    header = (root / "vco_base.hpp").read_text()
    types = {"square": "BandLimitedSquareOscillator", "saw": "BandLimitedSawOscillator",
             "triangle": "TriangleOscillator", "sine": "SineTableOscillator"}
    replacements = []
    for name, kind in types.items():
        if header.count("\t\t" + kind + " " + name + ";") != 1:
            raise SystemExit("Pinned concrete VCO value member mismatch: " + name)
        old = f"e.{name}.nextFromPhasor(e.phasor, phaseOffset + e.additionalPhaseOffset)"
        new = f"nextOwnedWave(e.{name}, e.phasor, phaseOffset + e.additionalPhaseOffset)"
        replacements.append((old, new, 1 if name == "sine" else 2))
    if helper in text:
        if text.count(helper) != 1 or any(old in text or text.count(new) != count
                                        for old, new, count in replacements):
            raise SystemExit("Incomplete existing waveform dispatch preparation")
        print("Full VCO waveform preparation already present")
        return
    needle = '#define POLY_INPUT "poly_input"'
    if text.count(needle) != 1 or "nextOwnedWave" in text:
        raise SystemExit("Pinned VCO helper seam mismatch")
    for old, new, count in replacements:
        if text.count(old) != count or new in text:
            raise SystemExit("Pinned VCO waveform call mismatch: " + old)
    text = text.replace(needle, helper + needle)
    for old, new, _ in replacements:
        text = text.replace(old, new)
    path.write_text(text)
    print("Prepared direct VCO-owned waveforms with unchanged phase arithmetic")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    source = parser.parse_args().source
    prepare(source)
    prepare_phase_cycles(source)
