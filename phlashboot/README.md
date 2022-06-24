## Simple standard binary test

This source builds a simple 'hello mum' program containing standard sections, then uses the
`esptool.py` from ESP-IDF to translate the output ELF into an ESP32S3 `.bin` format that
can be flashed directly in place of the esp-idf 2nd stage bootloader.bin.

### Fugly bits

I have avoided all use of esp-idf components/headers, as I wanted to see what a minimal
program could look like - turns out there are some fiddly bits to remember:

 * Calling into the ESP32-ROM requires knowledge of specific function addresses, which
   I obtained from the esp-idf 'unstable internals API', in particular from the linker
   script generated from Espressif internal information (interface-esp32s3.yml apparantly)
   and published in esp-idf repository:
   `$(ESP-IDF)/components/esp_rom/esp32s3/ld/esp32s3.rom.ld`
 * Calling ESP32-ROM functions needs to go via a function pointer, as otherwise the jump
   distance is too great for a direct `call<n>` instruction, so the linker b0rks.
 * The 1st stage bootloader in ROM enables 3 watchdogs (not two as documented!), both
   digital ones (MWDT in Timer0, RWDT) and the super watchdog (SWD). There is no documentation
   yet for the RWDT/SWD registers, so rummaging in esp-idf source was required to produce the
   `#define`'s in my code


### Flash & run

I used the `openocd` provided with esp-idf to flash the resulting `phlashboot.bin` into the
start of built-in flash memory:
```bash
% openocd -f boards/esp32s3-builtin.conf \
   -c 'init' \
   -c 'reset halt' \
   -c 'flash write_image build/phlashboot.bin 0' \
   -c 'exit'
```

Now with a monitor on the USB Serial port (`minicom -d /dev/ttyACO`), I hit the reboot button
and watch my code write 'Hello Mum!' to the console :)
