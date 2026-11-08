#include <appdef.h>
#include <sdk/os/debug.h>
#include <sdk/os/lcd.h>
APP_NAME("Launcher test: HH3")
APP_DESCRIPTION("Native HH3 SDK app. Displays a message and returns after a key press.")
APP_AUTHOR("Hollyhock Custom Launcher")
APP_VERSION("0.6.1")
int main() {
  LCD_ClearScreen();
  Debug_Printf(2, 3, false, 0, "HH3 .hh3 started");
  Debug_Printf(2, 5, false, 0, "Press any key to return");
  LCD_Refresh();
  Debug_WaitKey();
  return 0;
}
