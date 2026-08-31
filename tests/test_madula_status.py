"""Configuration contracts and host tests of the actual LED work handler."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class MadulaStatusTests(unittest.TestCase):
    def test_all_three_variants_keep_studio_cdc_and_rgb(self):
        build = (ROOT / "build.yaml").read_text()
        for variant in ("trackball", "trackpoint", "iqs"):
            block = next(b for b in build.split("  - board:")
                         if f"artifact-name: madula_{variant}" in b)
            for snippet in ("studio-rpc-usb-uart", "madula-dya-studio-v2",
                            "cornix-central-rgb-led", "cornix-cdc-boot",
                            "cornix-production-cdc", "zmk-usb-logging"):
                self.assertIn(snippet, block)

    def test_pmw_settings_are_only_in_trackball_extension(self):
        common = (ROOT / "snippets/madula-dya-studio-v2/madula-dya-studio-v2.conf").read_text()
        self.assertNotIn("CONFIG_ZMK_PMW3610", common)
        tb = (ROOT / "snippets/madula-dya-trackball/madula-dya-trackball.conf").read_text()
        self.assertIn("CONFIG_ZMK_PMW3610_STUDIO_RPC=y", tb)

    def test_widget_is_not_disabled_by_gpio_rgb_snippet(self):
        rgb = (ROOT / "snippets/cornix-central-rgb-led/cornix-central-rgb-led.conf").read_text()
        self.assertNotIn("CONFIG_RGBLED_WIDGET=n", rgb)
        conf = (ROOT / "boards/shields/madula_central/madula_central.conf").read_text()
        for setting in ("CONFIG_RGBLED_WIDGET=y", "CONFIG_RGBLED_WIDGET_CONN_SHOW_USB=y",
                        "CONFIG_MADULA_WS2812_LAYER_NUMBER=y",
                        "CONFIG_RGBLED_WIDGET_SHOW_LAYER_CHANGE=n"):
            self.assertIn(setting, conf)

    @unittest.skipUnless(shutil.which("cc"), "host C compiler required")
    def test_actual_layer_handler(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            # Generated header stubs live only in the temporary test directory.
            for name in ("zephyr/kernel.h", "zephyr/sys/atomic.h", "zephyr/logging/log.h",
                         "zmk/event_manager.h", "zmk/events/layer_state_changed.h",
                         "zmk/keymap.h", "zmk_rgbled_widget/widget.h"):
                header = root / name
                header.parent.mkdir(parents=True, exist_ok=True)
                header.write_text("/* host test stub */\n")
            binary = root / "layer-test"
            subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-I", str(root), str(ROOT / "tests/layer_number_host.c"),
                            "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
