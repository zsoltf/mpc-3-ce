#!/usr/bin/env python3
"""Regression checks for discovery ambiguity, changed bodies and RTTI conflict."""
import struct
import unittest
from collections import defaultdict
from firmware_reanchor import Matcher, normalize


class FixtureELF:
    """Small function/RTTI substitute; does not qualify the real ELF decoder."""
    def __init__(self, base, bodies, rtti=None):
        self.starts = [base]
        self.bodies = {}
        for body in bodies:
            self.bodies[self.starts[-1]] = body
            self.starts.append(self.starts[-1] + len(body))
        self.slots = defaultdict(list)
        self.vtables = defaultdict(list)
        for key, slot, function, count in rtti or []:
            self.slots[function].append((key, slot, 0x90000, count))
            self.vtables[key].append((0x90000, [function] * count))

    def at(self, address, length):
        for start, body in self.bodies.items():
            if start <= address and address + length <= start + len(body):
                return body[address-start:address-start+length]
        raise ValueError('unmapped fixture address')

    def mapped(self, address):
        # Real ELF load zero also includes scalar integers. They must not be
        # erased as pointer literals by the normalizer.
        return 0 <= address < 0x40000

    def bounds(self, address):
        for start, end in zip(self.starts, self.starts[1:]):
            if start <= address < end:
                return start, end
        return None


BODY = struct.pack('<4I', 0xe1a00000, 0xe1a01001, 0xe1a02002, 0xe12fff1e)
OTHER = struct.pack('<4I', 0xe1a03003, 0xe1a04004, 0xe1a05005, 0xe12fff1e)
KEY = ('N4juce9ComponentE', 0)


class DiscoveryTest(unittest.TestCase):
    def test_unique_whole_body_interior(self):
        match = Matcher(FixtureELF(0x10000, [BODY]), FixtureELF(0x20000, [BODY])).locate(0x10004)
        self.assertEqual((match['status'], match['candidates']), ('auto', ['0x20004']))
        self.assertEqual(match['layout_status'], 'unresolved')

    def test_repeated_body_is_not_auto(self):
        match = Matcher(FixtureELF(0x10000, [BODY]), FixtureELF(0x20000, [BODY, BODY])).locate(0x10000)
        self.assertEqual(match['status'], 'needs-trace')
        self.assertEqual(len(match['candidates']), 2)

    def test_changed_body_with_rtti_stays_trace(self):
        old = FixtureELF(0x10000, [BODY], [(KEY, 0, 0x10000, 1)])
        new = FixtureELF(0x20000, [OTHER], [(KEY, 0, 0x20000, 1)])
        match = Matcher(old, new).locate(0x10000)
        self.assertEqual((match['status'], match['candidates']), ('needs-trace', ['0x20000']))

    def test_changed_interior_has_owner_not_guessed_hook(self):
        old = FixtureELF(0x10000, [BODY], [(KEY, 0, 0x10000, 1)])
        new = FixtureELF(0x20000, [OTHER], [(KEY, 0, 0x20000, 1)])
        match = Matcher(old, new).locate(0x10004)
        self.assertEqual((match['status'], match['candidates']), ('needs-trace', []))
        self.assertEqual(match['rtti_owner_candidates'][0]['new_function'], '0x20000')

    def test_signature_rtti_disagreement_stays_trace(self):
        old = FixtureELF(0x10000, [BODY], [(KEY, 0, 0x10000, 1)])
        new = FixtureELF(0x20000, [BODY, OTHER], [(KEY, 0, 0x20010, 1)])
        match = Matcher(old, new).locate(0x10000)
        self.assertEqual(match['status'], 'needs-trace')
        self.assertEqual(match['reason'], 'signature disagrees with RTTI slot')

    def test_short_body_stays_trace(self):
        match = Matcher(FixtureELF(0x10000, [BODY[:8]]), FixtureELF(0x20000, [BODY[:8]])).locate(0x10000)
        self.assertEqual(match['status'], 'needs-trace')

    def test_small_scalars_are_not_pointer_literals(self):
        data = struct.pack('<4I', 0, 1, 40, 104)
        self.assertEqual(normalize(data, FixtureELF(0x10000, [BODY])), data)


if __name__ == '__main__':
    unittest.main()
