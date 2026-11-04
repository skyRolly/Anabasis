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
VERSION = "9.0.5"
TAG = "d60b7a78030661574a615b586d9325ba7a7fa7dd"
PREVIOUS = {
    "9.0.0": "f8f8864172464b9adf9eba6101e1f784838d1597",
    "9.0.1": "e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8",
    "9.0.4": "3fb5cbc0340758596dac39f7c8de47d562b4896e",
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
        self.assertIn("this tree pins JUCE 9.0.5", result.stdout)
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
        self.seed_cache("9.0.4", PREVIOUS["9.0.4"])
        self.assert_selected(self.configure(f"-DANABASIS_JUCE_PATH={self.juce}"),
                             "9.0.4", PREVIOUS["9.0.4"])

    def test_existing_904_cache_is_refused_before_dependency_boundary(self):
        self.seed_cache("9.0.4", PREVIOUS["9.0.4"])
        self.assert_stale(self.configure())


    def source_checkout(self):
        (self.juce / "CMakeLists.txt").write_text("# Minimal git checkout for source provenance\n")
        for args in (("init", "--quiet"), ("add", "CMakeLists.txt"),
                     ("-c", "user.name=Cache Test", "-c", "user.email=cache-test@example.invalid",
                      "commit", "--quiet", "-m", "fixture")):
            subprocess.run(["git", "-C", str(self.juce), *args], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return subprocess.check_output(
            ["git", "-C", str(self.juce), "rev-parse", "HEAD"], text=True).strip()

    def test_fetched_source_must_match_immutable_pin(self):
        actual = self.source_checkout()
        result = self.configure()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("JUCE source HEAD " + actual, " ".join(result.stdout.split()))
        self.assertIn("does not match immutable pin " + TAG, " ".join(result.stdout.split()))
        self.assertNotIn("verified JUCE source HEAD", result.stdout)

    def test_matching_custom_immutable_source_is_verified(self):
        actual = self.source_checkout()
        result = self.configure("-DANABASIS_JUCE_TAG=" + actual,
                                "-DANABASIS_JUCE_VERSION=custom-version")
        # The minimal checkout has no JUCE plugin CMake functions; reaching their
        # later error proves the provenance gate admitted this real custom SHA.
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("verified JUCE source HEAD " + actual, result.stdout)
        self.assertNotIn("does not match immutable pin", result.stdout)


    def archive_source(self):
        # Defining the first product API supplies a precise boundary marker:
        # its deliberate failure is after the real source verifier returns.
        (self.juce / "CMakeLists.txt").write_text(
            'function(juce_add_plugin)\n'
            '  message(FATAL_ERROR "JUCE_ARCHIVE_PRODUCT_BOUNDARY_REACHED")\n'
            'endfunction()\n', encoding="utf-8")

    def assert_archive_override(self, result):
        output = " ".join(result.stdout.split())
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("unverified JUCE source override", output)
        self.assertIn("JUCE_ARCHIVE_PRODUCT_BOUNDARY_REACHED", output)
        self.assertNotIn("verified JUCE source HEAD", output)
        self.assertNotIn("does not match immutable pin", output)

    def test_explicit_archive_override_remains_supported(self):
        self.archive_source()
        self.assert_archive_override(self.configure())

    def test_explicit_archive_override_does_not_require_git(self):
        self.archive_source()
        self.assert_archive_override(self.configure("-DCMAKE_DISABLE_FIND_PACKAGE_Git=TRUE"))

    def test_archive_override_does_not_inherit_ancestor_git_head(self):
        self.archive_source()
        (self.base / "parent.txt").write_text("Unrelated parent repository\n")
        for args in (("init", "--quiet"), ("add", "parent.txt"),
                     ("-c", "user.name=Cache Test", "-c", "user.email=cache-test@example.invalid",
                      "commit", "--quiet", "-m", "parent fixture")):
            subprocess.run(["git", "-C", str(self.base), *args], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        self.assert_archive_override(self.configure())

    def test_normal_source_without_git_metadata_is_refused(self):
        self.archive_source()
        result = subprocess.run(
            ["cmake", f"-Djuce_SOURCE_DIR={self.juce}", f"-DANABASIS_JUCE_TAG={TAG}",
             "-P", str(ROOT / "cmake/VerifyJuceSource.cmake")],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
            timeout=30, check=False)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("cannot verify JUCE source Git identity", " ".join(result.stdout.split()))
        self.assertNotIn("unverified JUCE source override", result.stdout)

    def test_broken_git_metadata_is_not_treated_as_an_archive(self):
        self.archive_source()
        (self.juce / ".git").write_text("gitdir: missing-git-directory\n")
        result = self.configure()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("cannot verify JUCE source Git identity", " ".join(result.stdout.split()))
        self.assertNotIn("JUCE_ARCHIVE_PRODUCT_BOUNDARY_REACHED", result.stdout)
        self.assertNotIn("unverified JUCE source override", result.stdout)


if __name__ == "__main__":
    unittest.main()
