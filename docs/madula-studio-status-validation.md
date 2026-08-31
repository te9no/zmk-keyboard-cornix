# Madula Studio / status LED integration — 2026-08-31

This incorporates the requested three-variant Studio and dual-LED features on
top of `main` at `667f7fbba4ee955c7fcd56cb2d67c7f5aff0c07c`. The earlier
uncommitted `feature/madula-tp-status` edits were not available in this checkout;
this is a reconstruction on current main, not a claim that those edits were merged.

## Changes

- Studio USB transport, runtime services and common diagnostics on TB, LPPS and IQS.
- PMW3610-specific settings/RPC stay in the trackball extension, not the common snippet.
- XIAO GPIO RGB remains independent of the existing SPI3 WS2812 status widget.
- WS2812 USB indication enabled (cyan when USB is the selected endpoint).
- Madula-only numbered layer pulses, using the pinned widget's sharing/priority API.
  Layer 0 is silent; layer N gives N white pulses, 120 ms on/off. A change of the
  highest layer replaces the sequence. Higher-priority status can suppress it.
- CDC logging and 1200-baud boot trigger retained on every variant.

Input driver revisions, pinouts, IQS rotation and LPPS free XY/deadzone settings
are unchanged. No upstream PR or external driver modification was made.

## Local verification

Using the workspace's `just.sh`, with the existing `madula-lpps-validation` west
dependencies, a separate `build-studio-status` build directory, and explicit
`ZMK_CONFIG_ROOT` / `ZMK_CONFIG_BRANCH=codex/madula-studio-status`:

```sh
./just.sh --profile madula-lpps-validation build-fast madula_ --pristine=always
./just.sh --profile madula-lpps-validation build-fast cornix_tps43_production --pristine=always
python3 -m unittest discover -s tests -v
```

All four firmware targets built successfully. All four host/configuration tests
passed, including execution of the actual LED handler against mocked scheduling
and widget APIs: layer indices 0–31, release to base, replacement mid-sequence,
unchanged highest layer, and higher-priority status before/during a sequence.
Mocks do not establish LED timing or electrical behavior on hardware.

Generated configuration confirms Studio UART transport, CDC debug/boot trigger,
GPIO RGB and WS2812 are enabled in all three Madula variants. PMW3610 custom RPC
is enabled only in TB. Studio and logging use distinct chosen UARTs. The board's
existing additional CDC node remains unchanged from the previous TB configuration.
TPS43 still has GPIO RGB enabled with the SPI widget disabled.
Generated LPPS DTS retains both deadzones at 1200 and settings override disabled;
IQS retains `CONFIG_INPUT_IQS9151_ROTATE_270=y`.

Local UF2 SHA-256 (CI paths/builds may produce different binaries):

| Target | SHA-256 |
| --- | --- |
| madula_trackball | `402737e258b163755d8f9411803cedc9d0a59a253d8a6ff14b0405a4ea6634e2` |
| madula_trackpoint | `d320d9e9ee3695cf0c717e47151c035a92bb6173a8cb021f7d2cdd209270a42d` |
| madula_iqs | `293da123b32df7c1c1dadd8c1bfe65190389fb857b0febeb9f3e7cca9f9d4293` |

## Hardware gates — not yet verified for this change

- LPPS and IQS: DYA Studio connection, keymap/runtime editing, input and split regression.
- TB: Studio sensor diagnostics and pointer regression with the new LED configuration.
- All three: independent GPIO RGB and SPI LED, USB indication, numbered layer pulses,
  idle power-off, and precedence of battery/connectivity indications.
- Recovery: CDC logs and 1200-baud reboot after installing each new image.

No device was flashed in this integration task. Prior pointer-input acceptance
does not complete the above new Studio/LED hardware gates.
