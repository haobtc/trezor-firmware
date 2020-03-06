/*
 * This file is part of the Trezor project, https://trezor.io/
 *
 * Copyright (C) 2014 Pavol Rusnak <stick@satoshilabs.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include <libopencm3/cm3/scb.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

#include "bootloader.h"
#include "buttons.h"
#include "layout.h"
#include "memory.h"
#include "oled.h"
#include "rng.h"
#include "setup.h"
#include "signatures.h"
#include "usb.h"
#include "util.h"
#include "mi2c.h"


void layoutFirmwareFingerprint(const uint8_t *hash) {
  char str[4][17] = {0};
  for (int i = 0; i < 4; i++) {
    data2hex(hash + i * 8, 8, str[i]);
  }
  layoutDialog(&bmp_icon_question, "Abort", "Continue", "Compare fingerprints",
               str[0], str[1], str[2], str[3], NULL, NULL);
}

bool get_button_response(void) {
  do {
    delay(100000);
    buttonUpdate();
  } while (!button.YesUp && !button.DownUp);
  return button.YesUp;
}

static void __attribute__((noreturn)) load_app(int signed_firmware) {
  // zero out SRAM
  memset_reg(_ram_start, _ram_end, 0);

  jump_to_firmware((const vector_table_t *)FLASH_PTR(FLASH_APP_START),
                   signed_firmware);
}

static void bootloader_loop(void) {
  bootloader_logo();
  usbLoop();
}

int main(void) {
#ifndef APPVER
  setup();
#endif
  __stack_chk_guard = random32();  // this supports compiler provided
                                   // unpredictable stack protection checks
#ifndef APPVER
  memory_protect();
  oledInit();
#endif
  mpu_config_bootloader();
#ifndef APPVER
  if(false == bCheckBleUpdate())
  {
    vDisp_PromptInfo(DISP_UPDATE_FAIL, true);
    while(false == get_button_response());
    scb_reset_core();
  }
  bool left_pressed = (buttonRead() & BTN_PIN_DOWN) == 0;


  if (firmware_present_new() && !left_pressed && (SESSION_FALG != *( uint32_t *)(BOOTLOAD_ADDR) ) ) {
    const image_header *hdr =
        (const image_header *)FLASH_PTR(FLASH_FWHEADER_START);

    uint8_t fingerprint[32] = {0};
    int signed_firmware = signatures_new_ok(hdr, fingerprint);
    if (SIG_OK != signed_firmware) {
      vDisp_PromptInfo(DISP_FIRMWARE_UNOFFICIAL, true);
      while(false == get_button_response());
      scb_reset_core();
    }

    if (SIG_OK != check_firmware_hashes(hdr)) {
      vDisp_PromptInfo(DISP_FIRMWARE_BROKEN, true);
      while(false == get_button_response());
      scb_reset_core();

    }

    mpu_config_off();
    load_app(signed_firmware);
  }
#endif

  bootloader_loop();

  return 0;
}
