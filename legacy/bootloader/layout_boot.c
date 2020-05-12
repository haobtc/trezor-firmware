#include "layout_boot.h"

void layoutBootHome(void) {
  static uint32_t system_millis_logo_refresh = 0;
  if (layoutNeedRefresh()) {
    oledClear();
    oledDrawBitmap(30, 20, &bmp_BiXin_logo32);
    oledDrawStringCenter(85, 20, "BiXin", FONT_STANDARD);
    oledDrawStringCenter(85, 30, "Bootloader", FONT_STANDARD);
    oledDrawStringCenter(85, 40,
                         VERSTR(VERSION_MAJOR) "." VERSTR(
                             VERSION_MINOR) "." VERSTR(VERSION_PATCH),
                         FONT_STANDARD);
    layoutFillBleName(7);
    oledRefresh();
  }
  // 1000 ms refresh
  if ((timer_ms() - system_millis_logo_refresh) >= 1000) {
#if !EMULATOR
    layoutStatusLogo();
    system_millis_logo_refresh = timer_ms();
#endif
  }
}
