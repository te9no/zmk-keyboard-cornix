### XIAO nRF52840 SoftDevice recovery (S140 v7)

The XIAO nRF52840 ZMK layout starts the application at `0x27000`, which
corresponds to S140 v7. If firmware built for a `0x1000` application offset is
flashed accidentally, it can overwrite the SoftDevice area while leaving the
UF2 bootloader available.

Use [`xiao_nrf52840_s140_7.3.0_restore_no_mbr.uf2`](./xiao_nrf52840_s140_7.3.0_restore_no_mbr.uf2)
to restore only `0x1000` through `0x26fff`. It does not replace the MBR or UF2
bootloader. The image was extracted from Adafruit's official
`xiao_nrf52840_ble_bootloader-0.11.0_s140_7.3.0.hex` release asset.

Recovery order:

1. Double-reset the XIAO to enter its UF2 bootloader.
2. Flash `xiao_nrf52840_s140_7.3.0_restore_no_mbr.uf2`.
3. Double-reset again and flash the intended Madula/ZMK application UF2.

SHA-256:

```text
e70303061fc316a3daa8d4c31ffe250b446326257f1676cd8eb546b8f032f4cb  xiao_nrf52840_s140_7.3.0_restore_no_mbr.uf2
```

Do not use an S140 v6 recovery image with this layout. S140 v6 uses a different
application offset (`0x26000`).
