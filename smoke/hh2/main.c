// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdint.h>
__attribute__((section(".hollyhock_name"))) const char name[] = "Launcher test: HH2";
__attribute__((section(".hollyhock_description"))) const char description[] = "OS 2000 native HHK/BIN test; press a key to return.";
__attribute__((section(".hollyhock_author"))) const char author[] = "Hollyhock Custom Launcher";
__attribute__((section(".hollyhock_version"))) const char version[] = "0.6.2";
// Pinned OS 2000 entrypoints from meta/hollyhock-3/versions/v02.01.2000.0000.
void test_main(void) {
  ((void (*)(void))0x800394c0)();
  ((void (*)(unsigned,unsigned,_Bool,int,const char *,...))0x8002dbc8)
      (1, 3, 1, 0, "HH2 launcher test: press a key");
  ((void (*)(void))0x8003733e)();
  (void)((int (*)(void))0x80094380)();
}
