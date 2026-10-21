#!/usr/bin/env python3
"""Exercise top-level CMake cache migration without downloading/building JUCE.

Copy-and-adapt provenance (ADR-0009): the dependency-boundary stand-in pattern
from Anamorph:tests/juce_cache_tests.py. The cases cover Anabasis's own pins.
"""

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
VERSION = "9.0.4"
TAG = "3fb5cbc0340758596dac39f7c8de47d562b4896e"
PREVIOUS = {
    "9.0.0": "f8f8864172464b9adf9eba6101e1f784838d1597",
    "9.0.1": "e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8",
}
MARKER = "ANABASIS_JUCE_CACHE_PROBE_REACHED"


class JuceCacheTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="anabasis-juce-cache-")
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.build = self.base / "build with spaces"
        self.juce = self.base / "local JUCE"
        self.juce.mkdir()
        (self.juce / "CMakeLists.txt").write_text(
            'file(WRITE "${CMAKE_BINARY_DIR}/selected-pin.txt" '
            '"${ANABASIS_JUCE_VERSION}\n${ANABASIS_JUCE_TAG}\n")\n'
            f'message(FATAL_ERROR "{MARKER}")\n',
            encoding="utf-8",
        )

    def seed_cache(self, version, tag):
        self.build.mkdir()
        (self.build / "CMakeCache.txt").write_text(
            f"ANABASIS_JUCE_VERSION:STRING={version}\n"
            f"ANABASIS_JUCE_TAG:STRING={tag}\n",
            encoding="utf-8",
        )

    def configure(self, *args):
        return subprocess.run(
            ["cmake", "-S", str(ROOT), "-B", str(self.build), "-G", "Ninja",
             f"-DFETCHCONTENT_SOURCE_DIR_JUCE={self.juce}", *args],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            timeout=60, check=False,
        )

    def assert_selected(self, result, version=VERSION, tag=TAG):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn(MARKER, result.stdout)
        self.assertEqual(
            (self.build / "selected-pin.txt").read_text().splitlines(),
            [version, tag],
        )

    def assert_stale(self, result):
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertNotIn(MARKER, result.stdout)
        self.assertIn("stale JUCE cache", result.stdout)
        self.assertIn("this tree pins JUCE 9.0.4", result.stdout)
        self.assertIn(
            "-UANABASIS_JUCE_TAG -UANABASIS_JUCE_VERSION",
            " ".join(result.stdout.split()),
        )
        self.assertFalse((self.build / "selected-pin.txt").exists())

    def test_fresh_and_repeated_configuration_select_current_pin(self):
        self.assert_selected(self.configure())
        self.assert_selected(self.configure())

    def test_previous_caches_fail_until_both_fields_are_reset(self):
        for version, tag in PREVIOUS.items():
            with self.subTest(version=version):
                self.build = self.base / version
                self.seed_cache(version, tag)
                self.assert_stale(self.configure())
                self.assert_stale(self.configure())
                cache = (self.build / "CMakeCache.txt").read_text()
                self.assertIn(f"ANABASIS_JUCE_VERSION:STRING={version}", cache)
                self.assertIn(f"ANABASIS_JUCE_TAG:STRING={tag}", cache)
                self.assert_selected(self.configure(
                    "-UANABASIS_JUCE_TAG", "-UANABASIS_JUCE_VERSION"))

    def test_resetting_only_one_field_still_refuses_the_other(self):
        for version, tag in PREVIOUS.items():
            for field in ("TAG", "VERSION"):
                with self.subTest(version=version, field=field):
                    self.build = self.base / f"{version}-{field}"
                    self.seed_cache(version, tag)
                    self.assert_stale(self.configure(f"-UANABASIS_JUCE_{field}"))

    def test_mixed_old_and_current_fields_are_refused(self):
        for version, tag in PREVIOUS.items():
            for i, pair in enumerate(((version, TAG), (VERSION, tag))):
                with self.subTest(version=version, pair=pair):
                    self.build = self.base / f"mixed-{version}-{i}"
                    self.seed_cache(*pair)
                    self.assert_stale(self.configure())

    def test_custom_pin_survives_reconfiguration(self):
        args = ("-DANABASIS_JUCE_TAG=custom-pin",
                "-DANABASIS_JUCE_VERSION=custom-version")
        self.assert_selected(self.configure(*args), "custom-version", "custom-pin")
        self.assert_selected(self.configure(), "custom-version", "custom-pin")

    def test_local_checkout_allows_intentional_old_pin_comparison(self):
        self.seed_cache("9.0.1", PREVIOUS["9.0.1"])
        self.assert_selected(self.configure(f"-DANABASIS_JUCE_PATH={self.juce}"),
                             "9.0.1", PREVIOUS["9.0.1"])


if __name__ == "__main__":
    unittest.main()
