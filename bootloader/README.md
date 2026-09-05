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

> [!WARNING]
> `s140_6.1.1_restore_no_mbr.uf2` is retained as a legacy artifact. Do not use
> it for the current XIAO/Madula layout: S140 v6 uses a different application
> offset (`0x26000`).

### Legacy SoftDevice recovery notes (S140 v6.1.1)

### 软设备恢复说明（中文）

由于原RMK固件移除了SoftDevice，在刷入ZMK固件之前，必须先恢复SoftDevice：

1. **先用SoftDevice恢复包刷入**，地址：[bootloader/s140_restore.uf2](./s140_6.1.1_restore_no_mbr.uf2)
2. **Recovery模式进入方法**：双击RESET键进入bootloader
3. **刷入zmk固件**：使用UF2拖拽方式刷入

**注意顺序**：先SoftDevice → 后ZMK固件

    
### Extracting SoftDevice (s140) for nRF52840

Download the bootloader for nRF52840 from:

- Official repository: https://github.com/joric/nrfmicro/wiki/Bootloader
- This bootloader is compatible with nRF52840-based keyboards using ZMK firmware

These commands extract the SoftDevice (s140) from the bootloader HEX file and convert it to UF2 format for flashing:

```bash
# Convert HEX to binary
arm-none-eabi-objcopy -I ihex -O binary pca10056_bootloader-0.2.11_s140_6.1.1.hex full_flash.bin

# Extract SoftDevice section
# Skip bootloader (4096 bytes, start address 0x1000)
# Count matches SoftDevice size (151016 bytes, end address: 0x25DE7 )
dd if=full_flash.bin of=s140_sd_only.bin bs=1 skip=4096 count=151016

# method 1: generate hex file,  flashing with nrfutil or nrf connect
arm-none-eabi-objcopy -O ihex -I binary s140_sd_only.bin s140_sd_only.hex
arm-none-eabi-objcopy --change-address 0x1000 s140_sd_only.hex  s140_sd_only.hex

# method 2: generate uf2 , flash by uf2 bootloader 
# Convert bin content to UF2 format with start address for flashing 
python3 uf2conv.py s140_sd_only.bin -f 0xada52840 -b 0x1000 -c -o s140_restore.uf2
```

nRF Connect for Desktop's Programer  is very useful for getting sd address and size from bootloader hex file.

<img width="736" height="691" alt="图片" src="https://github.com/user-attachments/assets/be124646-870a-48d6-9387-d7cb043f4848" />


### Extracting SoftDevice (s130) for nRF51

Command to extract SoftDevice (s130) section:

```bash
# Skip bootloader (4096 bytes)
dd if=full_flash.bin of=s132_sd_only.bin bs=1 skip=4096 count=155648
```
