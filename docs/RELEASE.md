Hollyhock Custom Launcher 0.6.1 – getrennte Ausgaben für HH2 und HH3.

| Download | Vorhandene Installation | Apps |
| --- | --- | --- |
| hhk2-launcher-0.6.1-os2000.zip | HH2 auf OS 02.01.2000.0000 | .hhk, .hh3 und kompatible .bin |
| hhk3-launcher-0.6.1-os7002.zip | HH3 auf OS 02.01.7002.0001 | .hh3 |

Beide Pakete enthalten eine fertig benannte `run.bin`, Testprogramme, Anleitung,
Prüfsummen, tatsächliche Testberichte und den vollständigen Quellcode mit SDK.
Einzelne `run-hh2-os2000.bin`/`run-hh3-os7002.bin` vor dem Kopieren in
`run.bin` umbenennen.

Gemeinsame Oberfläche, Touch-/Tastenbedienung und fünf zuletzt gestartete Apps.
HH2 und HH3 führen getrennte Verläufe. HH2 nutzt native OS-2000-Lader und den
ursprünglichen Casio-Abschlussdialog; HH3 behält den OS-7002-Rückgabeadapter.

Beide Varianten sind automatisiert geprüft. Der abschließende Test dieser
Ausgabe an echten HH2-/HH3-Geräten steht noch aus.

Korrektur: Die HH2-Ausgabe erkennt HHK, HH3 und BIN gemeinsam, auch bei
groß- oder gemischtgeschriebenen Endungen. Ein gemeinsamer Verzeichnisdurchlauf
ersetzt die getrennten OS-Wildcard-Suchen. HH3-Apps erhalten die OS-2000-
Symboltabelle; ihre eigenen OS-/Hardwareanforderungen gelten weiterhin.
