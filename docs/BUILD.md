# Build und Tests

Die gemeinsame Oberfläche liegt unter `src/classic/`. App-Suche, Verlaufsformat und
UI-Hinweise werden über `src/target.hpp` für HH2 oder HH3 ausgewählt. Die App-Lader
liegen getrennt unter `src/loaders/hh2/` und `src/loaders/elf/`.

Beide Ausgaben nutzen das unveränderte SDK-Submodule `meta/hollyhock-3` auf Commit
`1e483e395391d50d9bd2d21cee4beecc7589fb1c`. Es enthält OS-Adressen für 2000 und 7002.
Die Makefile liest die passende `addresses.json` direkt daraus.

## Quellen und Voraussetzungen

```sh
git clone --recurse-submodules https://github.com/Loewenkrieger/Hollyhock-Custom-Launcher.git
cd Hollyhock-Custom-Launcher
```

Bei einem bestehenden Checkout: `git submodule update --init`. Im vollständigen
Releasepaket sind die nötigen SDK-Quellen bereits unter `source/` enthalten.
GitHubs automatisch erzeugte Source-Archive füllen Submodules nicht aus.

Für Hosttests: Linux, Python 3, g++ mit C++23 und ASan/UBSan.
Für Calculator-Builds zusätzlich `sh4a_nofpueb-elf-g++` 15.2.0 samt Werkzeugen.
Das festgesetzte Toolchain-Image steht in `tools/release.json` und im Workflow.

## HH3 bauen

```sh
make TARGET=hh3 test
make -j2 TARGET=hh3 launcher smoke
make TARGET=hh3 target-test
make TARGET=hh3 package
```

Ergebnisse: `dist/hh3/run.7002.bin`, Debug-ELF und `smoke/hh3/launcher-test.hh3`.

## HH2 bauen

```sh
make TARGET=hh2 test
make -j2 TARGET=hh2 launcher smoke
make TARGET=hh2 target-test
make TARGET=hh2 package
```

Ergebnisse: `dist/hh2/run.2000.bin`, Debug-ELF und
`smoke/hh2/launcher-test.hhk` sowie `launcher-test.bin`. Zusätzlich wird die native HH3-Test-App für den
HH2-Build mitgebaut und dem Paket beigelegt.
`make hh2` und `make hh3` sind Kurzformen für Build und Test-App der jeweiligen
Variante. Ohne Angabe bleibt HH3 voreingestellt. Build- und Abhängigkeitsordner sind
getrennt; die gemeinsame SDK-Bibliothek wird automatisch gebaut.

`make TARGET=… clean` entfernt die Builddateien dieser Variante.
Das SDK separat mit `make -C meta/hollyhock-3/sdk clean` bereinigen.

## Prüfungen und Pakete

Host- und Zielprüfungen schreiben ihre tatsächlichen Ausgaben nach
`dist/hh2/tests-*.json` bzw. `dist/hh3/tests-*.json`.
Der Paketbau akzeptiert nur erfolgreiche Berichte für dieselbe Variante,
dieselben Quelldateien und dasselbe Ziel-Binary.

Unter `dist/release/` entstehen pro Variante ein eindeutig benanntes Binary,
Komplettpaket, `BUILD-hh2.json`/`BUILD-hh3.json` und eine eigene SHA256-Liste.
Im Komplettpaket heißt der Launcher bereits `run.bin`; beide enthalten den
vollständigen Quellstand einschließlich SDK und beider OS-Adressdateien.

GitHub Actions prüft beide Varianten in einer Matrix. Ein Versionstag passend zu
`tools/release.json` veröffentlicht beide Pakete gemeinsam. Nur der Release-Job
hat Schreibrechte. Auf `main` und bei Pull Requests entstehen Build-Artefakte.

## HH2-Ladevertrag

HHK-Dateien sind native statische SuperH-ELF32-Dateien mit Big-Endian-Daten.
Der Lader prüft Section-Tabelle, Metadaten, Eintrittspunkt, Überlappungen und
Speichergrenzen vollständig vor dem ersten RAM-Schreibzugriff. PROGBITS werden
kopiert, NOBITS werden genullt. Auch gestrippte HHKs sind möglich, sofern ihre
Section-Tabelle erhalten bleibt. Absolute OS-Adressen werden unverändert benutzt.

BIN-Dateien folgen dem HH2-Format: Ladeadresse als Big-Endian-Wert bei Byte 12,
falls dieses Feld mit `0x8c` beginnt; sonst `0x8cff0000`. Der Eintritt liegt am
Dateianfang. Optionale vier Textfelder ab Byte 16 liefern Name, Beschreibung,
Autor und Version. Fehlende Textfelder führen zum Dateinamen als Anzeige.

Zulässige App-Bereiche: `0x8c200000–0x8cfcffff` und `0x8cff0000–0x8cffffff`.
Der OS-Arbeitsbereich und der Launcher bleiben ausgespart. Diese Bereichsprüfung
ersetzt keine Isolation einer laufenden App. Geladener Code erhält wie beim
ursprünglichen HH2 direkten Hardwarezugriff. Cache-Zeilen werden vor dem Aufruf
zurückgeschrieben und invalidiert.

HH2 benutzt beim Beenden den ursprünglichen nativen Casio-Abschlussdialog.
Der geprüfte stille OS-7002-Rückgabeadapter bleibt für HH3 erhalten.

## Verlauf und Schrift

MCS-Ordner: `hhlaunch`. HH3 nutzt `recent` mit `HH3REC1`; HH2 nutzt
`recent2` mit `HH2REC1`. Höchstens fünf Pfade, 512 Bytes, Schreiben erst nach
erfolgreichem App-Laden. Die bestehende HH3-Historie bleibt lesbar. Einzelheiten
zur Byte-Typausgabe: [MCS-ABI.md](MCS-ABI.md).

Nur zum Neuerzeugen der mitgelieferten Schrift wird Pillow benötigt:

```sh
python3 generate_classic_font.py assets/DejaVuSans.ttf src/classic/font.hpp
```

## Gemeinsame Suche und HH3 unter HH2

Beide Builds durchsuchen das Hauptverzeichnis einmal mit `\fls0\*` und
filtern die Endungen selbst ohne Beachtung der Groß-/Kleinschreibung. HH2
verwendet HHK-, BIN- und HH3-Lader, HH3 weiterhin den HH3-Lader. Erfolgreich
geöffnete Suchhandles werden auch bei Fehlern geschlossen.

Der HH3-Lader prüft alle PT_LOAD-Bereiche und den SH-Eintrittspunkt vor dem
ersten RAM-Schreibzugriff. Der eigene Launcher-Bereich bleibt ausgespart.
Der Aufruf übergibt `argc`, den Dateipfad als `argv[0]` und `environ`; die
Symboltabelle darin gehört zum tatsächlich gestarteten OS. Der unveränderte
SDK-Resolver wird in Hosttests mit den jeweiligen OS-Adressen geprüft.
HH3-Apps mit zwingenden OS-7002-Abhängigkeiten bleiben auf OS 2000 inkompatibel.
