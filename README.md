# Hollyhock Custom Launcher

A ClassPad-style launcher for the Casio ClassPad II, with app details and touch and keypad controls. **Version 0.6.2** is available for HH2 and HH3.

| Your installation | Apps | Download |
| --- | --- | --- |
| **HH3 · OS 02.01.7002.0001** | `.hh3` | [HH3 package](https://github.com/Loewenkrieger/Hollyhock-Custom-Launcher/releases/download/v0.6.2/hhk3-launcher-0.6.2-os7002.zip) |
| **HH2 · OS 02.01.2000.0000** | `.hhk`, `.hh3`, compatible `.bin` | [HH2 package](https://github.com/Loewenkrieger/Hollyhock-Custom-Launcher/releases/download/v0.6.2/hhk2-launcher-0.6.2-os2000.zip) |

HH2 recognizes all three extensions regardless of case. HH3 apps must support your installed OS version.

## Install

1. Download the package for your installed Hollyhock and OS version.
2. Back up your existing `run.bin`, then replace it with the one from the package. Copy your apps to the root directory too.
3. Safely eject the calculator, disconnect USB and open the launcher as usual.

## Use

Choose an app from the dropdown and tap **Run**. **Close**, **X** or **Clear** closes the launcher. You can also use the arrow keys and **EXE**.

Your five most recently launched apps appear at the top with a **\***. The last app is selected automatically, and your history is saved between sessions.

![Launcher preview](docs/preview/classic-closed.png)

[German guide](docs/README_DE.md) · [Build and tests (German)](docs/BUILD.md) · [Test report](docs/TEST-REPORT.txt)

Based on [YAL](https://github.com/ClasspadDev/yal), the [Hollyhock SDK](https://github.com/ClasspadDev/hollyhock-3) and the [Hollyhock 2](https://github.com/SnailMath/hollyhock-2) file formats.
Design inspired by [CP-Canvas](https://github.com/TheRainbowPhoenix/CP-Canvas).
