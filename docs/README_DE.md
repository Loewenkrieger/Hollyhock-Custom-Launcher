# Hollyhock Custom Launcher 0.7.0-beta.1

Lokale Beta mit Compact-Oberfläche: Schwarz-Weiß, runde Knöpfe und grauer Listenrahmen. Beim Öffnen erscheint Recent mit höchstens fünf zuletzt gestarteten Apps. Rechts liegt All apps mit der vollständigen Liste.

| Ausgabe | Vorhandenes System | Apps |
| --- | --- | --- |
| HH2 | OS 02.01.2000.0000 mit HH2 | `.hhk`, `.hh3`, kompatible `.bin` |
| HH3 | OS 02.01.7002.0001 mit HH3 | `.hh3` |

HH3-Apps müssen zur vorhandenen OS-Version passen. Die Schreibweise der Dateiendung spielt keine Rolle. Es wird nur der Launcher ausgetauscht.

## Installation

1. Die bisherige `run.bin` auf dem PC sichern.
2. Das passende lokale Paket entpacken: `hhk2-launcher-0.7.0-beta.1-os2000.zip` oder `hhk3-launcher-0.7.0-beta.1-os7002.zip`.
3. Die darin ganz oben liegende `run.bin` ins Hauptverzeichnis des Taschenrechner-Laufwerks kopieren und ersetzen.
4. Apps ebenfalls ins Hauptverzeichnis kopieren. Sicher auswerfen und USB trennen.
5. Den Launcher wie bisher öffnen. Die Fußzeile zeigt HH2 oder HH3 und `0.7.0-beta.1`.

Einzelne `run-hh2-os2000.bin`/`run-hh3-os7002.bin` vor dem Kopieren in `run.bin` umbenennen.

## Bedienung

| Eingabe | Funktion |
| --- | --- |
| Recent / All apps antippen | Seite wechseln |
| Pfeil links / rechts | Recent / All apps öffnen |
| Zeile antippen oder Pfeil hoch / runter | App auswählen |
| Liste ziehen | Scrollen, ohne eine App zu starten |
| Schmalen Scrollbalken ziehen | Schnell zu entfernten Einträgen springen |
| Run app / EXE | Ausgewählte App starten |
| Info (0) / Taste 0 | App-Details öffnen |
| In Details ziehen oder Pfeile hoch / runter | Beschreibung scrollen |
| Pfeile links / rechts in Details | Eine Textseite zurück / weiter |
| Clear / Backspace / X | Details schließen; sonst Launcher beenden |

Auswahl und Scrollposition bleiben beim Wechsel zwischen Recent und All apps erhalten, solange der Launcher geöffnet ist. Ein Antippen wählt nur aus; gestartet wird mit Run app oder EXE. In gestarteten Apps verarbeitet die App selbst alle Eingaben.

Ohne Verlauf ist Recent zunächst leer. Über All apps lässt sich die erste App starten. Der Verlauf wird erst nach erfolgreichem Laden aktualisiert, bleibt auf fünf Einträge begrenzt und verwendet die bisherigen getrennten HH2-/HH3-Variablen. Öffnen, Scrollen und Schließen schreiben nichts ins MCS.

## Prüfen und zurückwechseln

Zuerst den Launcher mit X schließen und erneut öffnen. Danach eine beiliegende Test-App verwenden: `smoke/launcher-test.hh3` für HH3 oder `smoke/launcher-test.hhk`/`launcher-test.bin` für HH2. Das HH2-Paket enthält zusätzlich die HH3-Test-App. Zum Zurückwechseln die gesicherte `run.bin` zurückkopieren.

HH2 verwendet weiterhin den ursprünglichen Casio-Abschlussdialog. HH3 nutzt den vorhandenen OS-7002-Rückgabeadapter. Formate, SDK und Adresstabellen bleiben erhalten.

Beide Ausgaben sind automatisiert geprüft. Ein Test auf echter HH2-/HH3-Hardware steht noch aus. Die tatsächlichen Ergebnisse liegen im Paket unter `tests-host.json` und `tests-target.json`.

## Paketinhalt

`run.bin`, Test-Apps, Anleitung, Prüfberichte, Prüfsummen, native Vorschauen und Debug-ELF. Vollständige Quellen einschließlich SDK liegen unter `source/`; siehe [Build und Tests](BUILD.md).

SDK-Lizenz: `source/meta/hollyhock-3/LICENSE`. Schriftlizenz: `source/src/classic/FONT-LICENSE.txt`.
