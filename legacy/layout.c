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

#include "layout.h"
#include "ble.h"
#include "chinese.h"
#include "oled.h"
#include "prompt.h"

#if !EMULATOR
#include "sys.h"
#include "timer.h"
#endif

static bool refresh_home = true;

bool layoutNeedRefresh(void) {
  if (refresh_home) {
    refresh_home = false;
    return true;
  }
  return false;
}
void layoutRefreshSet(bool refresh) { refresh_home = refresh; }

void layoutButtonNo(const char *btnNo, const BITMAP *icon) {
  int icon_width = 0;
  if (icon) {
    oledDrawBitmap(1, OLED_HEIGHT - 8, icon);
    icon_width = icon->width;
  }
  oledDrawString(3 + icon_width, OLED_HEIGHT - 8, btnNo, FONT_STANDARD);
  oledInvert(0, OLED_HEIGHT - 9,
             icon_width + oledStringWidth(btnNo, FONT_STANDARD) + 4,
             OLED_HEIGHT - 1);
}

void layoutButtonYes(const char *btnYes, const BITMAP *icon) {
  int icon_width = 0;
  if (icon) {
    oledDrawBitmap(OLED_WIDTH - 8 - 1, OLED_HEIGHT - 8, icon);
    icon_width = icon->width;
  }
  oledDrawStringRight(OLED_WIDTH - icon_width - 3, OLED_HEIGHT - 8, btnYes,
                      FONT_STANDARD);
  oledInvert(
      OLED_WIDTH - oledStringWidth(btnYes, FONT_STANDARD) - icon_width - 4,
      OLED_HEIGHT - 9, OLED_WIDTH - 1, OLED_HEIGHT - 1);
}

void layoutDialog(const BITMAP *icon, const char *btnNo, const char *btnYes,
                  const char *desc, const char *line1, const char *line2,
                  const char *line3, const char *line4, const char *line5,
                  const char *line6) {
  int left = 0;
  oledClear();
  if (icon) {
    oledDrawBitmap(0, 0, icon);
    left = icon->width + 4;
  }
  if (line1) oledDrawString(left, 0 * 9, line1, FONT_STANDARD);
  if (line2) oledDrawString(left, 1 * 9, line2, FONT_STANDARD);
  if (line3) oledDrawString(left, 2 * 9, line3, FONT_STANDARD);
  if (line4) oledDrawString(left, 3 * 9, line4, FONT_STANDARD);
  if (desc) {
    oledDrawStringCenter(OLED_WIDTH / 2, OLED_HEIGHT - 2 * 9 - 1, desc,
                         FONT_STANDARD);
    if (btnYes || btnNo) {
      oledHLine(OLED_HEIGHT - 21);
    }
  } else {
    if (line5) oledDrawString(left, 4 * 9, line5, FONT_STANDARD);
    if (line6) oledDrawString(left, 5 * 9, line6, FONT_STANDARD);
    if (btnYes || btnNo) {
      oledHLine(OLED_HEIGHT - 13);
    }
  }
  if (btnNo) {
    layoutButtonNo(btnNo, &bmp_btn_cancel);
  }
  if (btnYes) {
    layoutButtonYes(btnYes, &bmp_btn_confirm);
  }
  oledRefresh();
}

void layoutinfoCenter(const char *line1, const char *line2, const char *line3,
                      const char *line4, const char *line5, const char *line6) {
  oledClear();

  if (line1) oledDrawStringCenter(OLED_WIDTH / 2, 1 * 9, line1, FONT_STANDARD);
  if (line2) oledDrawStringCenter(OLED_WIDTH / 2, 2 * 9, line2, FONT_STANDARD);
  if (line3) oledDrawStringCenter(OLED_WIDTH / 2, 3 * 9, line3, FONT_STANDARD);
  if (line4) oledDrawStringCenter(OLED_WIDTH / 2, 4 * 9, line4, FONT_STANDARD);
  if (line5) oledDrawStringCenter(OLED_WIDTH / 2, 5 * 9, line5, FONT_STANDARD);
  if (line6) oledDrawStringCenter(OLED_WIDTH / 2, 6 * 9, line6, FONT_STANDARD);

  oledRefresh();
}

void layoutProgressUpdate(bool refresh) {
  static uint8_t step = 0;
  switch (step) {
    case 0:
      oledDrawBitmap(40, 0, &bmp_gears0);
      break;
    case 1:
      oledDrawBitmap(40, 0, &bmp_gears1);
      break;
    case 2:
      oledDrawBitmap(40, 0, &bmp_gears2);
      break;
    case 3:
      oledDrawBitmap(40, 0, &bmp_gears3);
      break;
  }
  step = (step + 1) % 4;
  if (refresh) {
    oledRefresh();
  }
}

void layoutProgressPercent(int permil) {
  char percent_asc[5] = {0};
  int i = 0;
  if (permil < 10) {
    percent_asc[i++] = permil + 0x30;
  } else if (permil < 100) {
    percent_asc[i++] = permil / 10 + 0x30;
    percent_asc[i++] = permil % 10 + 0x30;
  } else {
    permil = 100;
    percent_asc[i++] = permil / 100 + 0x30;
    percent_asc[i++] = permil % 100 / 10 + 0x30;
    percent_asc[i++] = permil % 10 + 0x30;
  }
  percent_asc[i] = '%';
  oledDrawStringCenter(60, 20, percent_asc, FONT_STANDARD);
}

void layoutProgress(const char *desc, int permil) {
  oledClear();
  layoutProgressPercent(permil / 10);
  // progressbar
  oledFrame(0, OLED_HEIGHT - 8, OLED_WIDTH - 1, OLED_HEIGHT - 1);
  oledBox(1, OLED_HEIGHT - 7, OLED_WIDTH - 2, OLED_HEIGHT - 2, 0);
  permil = permil * (OLED_WIDTH - 4) / 1000;
  if (permil < 0) {
    permil = 0;
  }
  if (permil > OLED_WIDTH - 4) {
    permil = OLED_WIDTH - 4;
  }
  oledBox(2, OLED_HEIGHT - 6, 1 + permil, OLED_HEIGHT - 3, 1);
  // text
  oledBox(0, OLED_HEIGHT - 16, OLED_WIDTH - 1, OLED_HEIGHT - 16 + 7, 0);
  if (desc) {
    oledDrawStringCenter(OLED_WIDTH / 2, OLED_HEIGHT - 16, desc, FONT_STANDARD);
  }
  oledRefresh();
}

#if !EMULATOR
void layoutStatusLogo(void) {
  static bool nfc_status_bak = false;
  static bool ble_status_bak = false;
  static bool usb_status_bak = false;
  static uint32_t counter = 0, counter_bak = 0;
  uint8_t pad = 16;

  if (sys_nfcState() == true) {
    nfc_status_bak = true;
    oledDrawBitmap(OLED_WIDTH - 3 * LOGO_WIDTH - pad, 0, &bmp_nfc);
  } else if ((true == nfc_status_bak) && (sys_nfcState() == false)) {
    nfc_status_bak = false;
    oledClearBitmap(OLED_WIDTH - 3 * LOGO_WIDTH - pad, 0, &bmp_nfc);
  }
  if (sys_bleState() == true) {
    ble_status_bak = true;
    oledDrawBitmap(OLED_WIDTH - 2 * LOGO_WIDTH - pad, 0, &bmp_ble);
  } else if ((true == ble_status_bak) && (sys_bleState() == false)) {
    ble_status_bak = false;
    oledClearBitmap(OLED_WIDTH - 2 * LOGO_WIDTH - pad, 0, &bmp_ble);
  }
  if (sys_usbState() == true) {
    usb_status_bak = true;
    oledDrawBitmap(OLED_WIDTH - LOGO_WIDTH - pad, 0, &bmp_usb);
  } else if ((true == usb_status_bak) && (sys_usbState() == false)) {
    usb_status_bak = false;
    oledClearBitmap(OLED_WIDTH - LOGO_WIDTH - pad, 0, &bmp_usb);
  }

  counter = timer_out_get(timer_out_countdown) / timer1s;
  if (counter_bak != counter) {
    uint8_t asc_buf[3] = {0};
    oledBox(0, 0, 16, 8, false);
    counter_bak = counter;
    asc_buf[0] = counter / 10 + 0x30;
    asc_buf[1] = counter % 10 + 0x30;
    if (counter > 0) {
      oledDrawString(0, 0, (char *)asc_buf, FONT_STANDARD);
    }
  }

  switch (battery_cap) {
    case 0:
      oledDrawBitmap(OLED_WIDTH - 16, 0, &bmp_battery_0);
      break;
    case 1:
      oledDrawBitmap(OLED_WIDTH - 16, 0, &bmp_battery_1);
      break;
    case 2:
      oledDrawBitmap(OLED_WIDTH - 16, 0, &bmp_battery_2);
      break;
    case 3:
      oledDrawBitmap(OLED_WIDTH - 16, 0, &bmp_battery_3);
      break;
    case 4:
      oledDrawBitmap(OLED_WIDTH - 16, 0, &bmp_battery_4);
      break;
    default:
      oledClearBitmap(OLED_WIDTH - 16, 0, &bmp_battery_0);
      break;
  }
  oledRefresh();
}

void layoutBlePasskey(uint8_t *passkey) {
  oledClear();
  oledDrawStringCenter(60, 20, "Bluetooth passkey:", FONT_STANDARD);
  oledDrawStringCenter(60, 30, (char *)passkey, FONT_DOUBLE);
  oledRefresh();
}

#endif

void layoutFillBleName(uint8_t line) {
  if (line < (OLED_HEIGHT / 8)) {
    if (ble_name_state() == true) {
      oledclearLine(line);
      oledDrawStringCenter(64, line * 8, ble_get_name(), FONT_STANDARD);
    }
  }
}

void layoutFillBleVersion(uint8_t line) {
  if (line < (OLED_HEIGHT / 8)) {
    if (ble_ver_state())
      oledDrawStringCenter(64, line * 8, ble_get_ver(), FONT_STANDARD);
  }
}

extern void shutdown(void);

void layoutError(const char *line1, const char *line2) {
  layoutDialog(&bmp_icon_error, NULL, NULL, NULL, line1, line2, NULL,
               "Your device", "will reset.", NULL);
  shutdown();
}

#if !EMULATOR
void layoutOperationWithCountdown(const char *info, uint32_t counter) {
  timer_out_set(timer_out_countdown, counter);
  oledClear();
  oledDrawStringCenter(OLED_WIDTH / 2, 30, info, FONT_STANDARD);
  oledRefresh();
}
#endif

// layout chinese
void layoutButtonNo_zh(const char *btnNo, const BITMAP *icon) {
  int icon_width = 0;
  if (icon) {
    oledDrawBitmap(1, OLED_HEIGHT - 8 - 1, icon);
    icon_width = icon->width;
  }
  oledDrawString_zh(3 + icon_width, OLED_HEIGHT - 12, (uint8_t *)btnNo);
  oledInvert(0, OLED_HEIGHT - 12,
             icon_width + oledStringWidth_zh((uint8_t *)btnNo) + 4,
             OLED_HEIGHT);
}

void layoutButtonYes_zh(const char *btnYes, const BITMAP *icon) {
  int icon_width = 0;
  if (icon) {
    oledDrawBitmap(OLED_WIDTH - 8 - 1, OLED_HEIGHT - 8, icon);
    icon_width = icon->width;
  }
  oledDrawStringRight_zh(OLED_WIDTH - icon_width - 3, OLED_HEIGHT - 12,
                         (uint8_t *)btnYes);
  oledInvert(
      OLED_WIDTH - oledStringWidth_zh((uint8_t *)btnYes) - icon_width - 4,
      OLED_HEIGHT - 12, OLED_WIDTH, OLED_HEIGHT);
}

void layoutDialog_zh(const BITMAP *icon, const char *btnNo, const char *btnYes,
                     const char *desc, const char *line1, const char *line2,
                     const char *line3) {
  int left = 0;
  oledClear();
  if (icon) {
    oledDrawBitmap(0, 0, icon);
    left = icon->width + 4;
  }
  if (line1) oledDrawString_zh(left, 0 * 13, (uint8_t *)line1);
  if (line2) oledDrawString_zh(left, 1 * 13, (uint8_t *)line2);
  if (line3) oledDrawString_zh(left, 2 * 13, (uint8_t *)line3);
  if (desc) {
  } else {
    if (btnYes || btnNo) {
      oledHLine(OLED_HEIGHT - 14);
    }
  }
  if (btnNo) {
    layoutButtonNo_zh(btnNo, &bmp_btn_cancel);
  }
  if (btnYes) {
    layoutButtonYes_zh(btnYes, &bmp_btn_confirm);
  }
  oledRefresh();
}

void layoutProgress_zh(const char *desc, int permil) {
  oledClear();
  layoutProgressPercent(permil / 10);
  // progressbar
  oledFrame(0, OLED_HEIGHT - 8, OLED_WIDTH - 1, OLED_HEIGHT - 1);
  oledBox(1, OLED_HEIGHT - 7, OLED_WIDTH - 2, OLED_HEIGHT - 2, 0);
  permil = permil * (OLED_WIDTH - 4) / 1000;
  if (permil < 0) {
    permil = 0;
  }
  if (permil > OLED_WIDTH - 4) {
    permil = OLED_WIDTH - 4;
  }
  oledBox(2, OLED_HEIGHT - 6, 1 + permil, OLED_HEIGHT - 3, 1);
  // text
  oledBox(0, OLED_HEIGHT - 16, OLED_WIDTH - 1, OLED_HEIGHT - 16 + 7, 0);
  if (desc) {
    oledDrawStringCenter_zh(OLED_WIDTH / 2, OLED_HEIGHT - 24, (uint8_t *)desc);
  }
  oledRefresh();
}
