# Hollyhock Custom Launcher 0.6.1

Für den Casio ClassPad II mit bereits installiertem Hollyhock. Wähle die Ausgabe,
die zu deiner vorhandenen Installation passt:

| Paket | Vorhandenes System | Unterstützte Apps |
| --- | --- | --- |
| HH3 | OS 02.01.7002.0001 mit HH3 | `.hh3` |
| HH2 | OS 02.01.2000.0000 mit HH2 | `.hhk`, `.hh3`, kompatible `.bin` |

## Installation

1. Die bisherige `run.bin` auf dem PC sichern.
2. Das passende [Releasepaket](https://github.com/Loewenkrieger/Hollyhock-Custom-Launcher/releases/tag/v0.6.1) entpacken.
3. Die darin ganz oben liegende `run.bin` ins Hauptverzeichnis des Taschenrechner-Laufwerks kopieren und ersetzen.
4. Apps ebenfalls ins Hauptverzeichnis kopieren, das Laufwerk sicher auswerfen und USB trennen.
5. Den Launcher über das System-Menü wie bisher öffnen.

Die einzeln angebotenen Dateien `run-hh2-os2000.bin` und `run-hh3-os7002.bin`
vor dem Kopieren in `run.bin` umbenennen. Es wird nur der Launcher ausgetauscht.

Die Fußzeile zeigt **HH2 | 0.6.1** oder **HH3 | 0.6.1**. Zum ersten Prüfen mit Close
schließen und erneut öffnen; dann die passende beiliegende Test-App starten:
`smoke/launcher-test.hh3` für HH3 oder `smoke/launcher-test.hhk`/`launcher-test.bin`
für HH2. Die HH2-Ausgabe enthält zusätzlich dieselbe `.hh3`-Test-App.
Die Test-App wartet auf eine Taste und kehrt zurück.

## Bedienung

| Eingabe | Funktion |
| --- | --- |
| Programmauswahl antippen | Dropdown öffnen oder schließen |
| Eintrag antippen | App auswählen |
| Im Dropdown ziehen oder Pfeile antippen | Liste scrollen |
| Run | Ausgewählte App starten |
| Close oder X | Launcher schließen |
| Pfeil hoch / runter | Vorherige / nächste App wählen |
| EXE bei geschlossenem Dropdown | App starten; bei Fokus auf Close schließen |
| EXE bei geöffnetem Dropdown | Auswahl bestätigen |
| Pfeil links / rechts bei geschlossenem Dropdown | Fokus zwischen Auswahl, Run und Close wechseln |
| Pfeil links / rechts bei geöffnetem Dropdown | Eine Seite zurück / weiter |
| Clear oder Backspace | Dropdown schließen; sonst Launcher schließen |

Apps werden im Hauptverzeichnis (`\fls0\`) gesucht. HH2 zeigt `.hhk`, `.hh3` und `.bin`; HH3 zeigt `.hh3`.
Die Schreibweise der Endung spielt keine Rolle. Die eigene `run.bin` wird nicht als HH2-App angeboten.
Innerhalb gestarteter Apps verarbeitet die App selbst alle Eingaben.

HH3 kehrt unter dem geprüften OS-7002-Aufrufer direkt zur vorherigen Casio-Ansicht
zurück. HH2 verwendet den ursprünglichen Casio-Abschlussdialog; diesen nach dem
Beenden bestätigen.

## Zuletzt gestartet

Die fünf zuletzt gestarteten Apps stehen mit einem **\*** oben. Die übrigen
folgen ohne doppelte Einträge in ihrer gewohnten Reihenfolge. Die letzte App ist
beim Öffnen ausgewählt; **EXE** startet sie erneut.

Der Verlauf wird im MCS erst nach erfolgreichem Laden aktualisiert. Öffnen,
Scrollen und Schließen schreibt nichts. Entfernte oder umbenannte Apps werden
übersprungen. Bei Speicherfehlern bleibt die normale Auswahl nutzbar.
HH2 und HH3 speichern getrennt; bestehende HH3-Einträge bleiben erhalten.

![Dropdown mit zuletzt gestarteten Apps](preview/classic-recent.png)

## HH2-Kompatibilität

HHK-Dateien werden nativ mit ihren OS-2000-Adressen geladen. Es findet keine
Übersetzung alter Apps auf OS 7002 statt. HH3-Dateien verwenden den ELF-Lader
mit der OS-2000-Symboltabelle. Apps, die zwingend ein neueres OS benötigen,
werden durch das Dateiformat allein nicht kompatibel. BINs müssen dem HH2-Ladeformat und den
zulässigen App-Speicherbereichen entsprechen. Bei ungültigen Dateien erscheint
eine Fehlermeldung. Weitere Details: [Build und Tests](BUILD.md).

## Teststand und Zurückwechseln

Automatisiert geprüft werden beide Oberflächen, Eingaben, Suche, Verlauf,
Startablauf und die jeweiligen Lader. Die HH2-Prüfungen testen auch fehlerhafte
Dateien und die tatsächlichen Kopier-/Nullzugriffe auf simulierten App-RAM.
Die SH-Builds und Testprogramme werden zusätzlich geprüft.

Ein abschließender Test auf echter HH2- und HH3-Hardware ist für diese Ausgabe
noch offen, einschließlich dauerhafter MCS-Speicherung und realer Ladezeiten.
Die tatsächlichen Testausgaben liegen im Paket unter `tests-host.json` und
`tests-target.json`. Zum Zurückwechseln die gesicherte `run.bin` zurückkopieren.

## Paketinhalt

`run.bin`, passende Test-App(s), Anleitung, Prüfbericht, Build-Angaben,
SHA256-Prüfsummen, Vorschauen und Debug-ELF. Der vollständige Quellcode mit SDK
liegt unter `source/`, die Build-Anleitung unter `source/docs/BUILD.md`.

Upstream-Lizenz unter `source/meta/hollyhock-3/LICENSE`, Schriftlizenz unter
`source/src/classic/FONT-LICENSE.txt`. Grundlagen: ClasspadDev/yal,
ClasspadDev/hollyhock-3 und SnailMath/hollyhock-2.
