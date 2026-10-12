# Minesweeper für Pebble Time 2

Minesweeper-Klon für die **Pebble Time 2** (Plattform `emery`, 200×228, Farbe).
Läuft zusätzlich auf `basalt`, `diorite` und `flint`. Kein Touch – reine Tastensteuerung.

## Features

- 8×8 Spielfeld (64 Felder)
- 3 Schwierigkeitsgrade: **Leicht** (6 Bomben), **Mittel** (10), **Schwer** (14)
- Erster Klick ist immer sicher (inkl. Nachbarfelder)
- Steuerungs-Übersicht beim Start (4 s oder Tastendruck), danach Schwierigkeitsauswahl
- Explosions-Animation bei Bombe (Blitz, Feuerball, Druckwelle, Trümmer, „BOOM!“)
- Sieges-Animation (Konfetti + Banner)
- Neuer Highscore: Pokal-Animation mit rotierenden Strahlen und Funkeln
- Highscore-Liste: Top 5 Zeiten (Sekunden) pro Schwierigkeitsgrad, dauerhaft gespeichert
- Einstellungen: **Display immer an** (Backlight dauerhaft)
- Spielende-Menü: *Neues Spiel* (Schwierigkeitswahl), *Nochmal*, *Beenden*

## Steuerung

| Taste | Aktion |
|---|---|
| UP / DOWN | Cursor ein Feld zurück / weiter (zeilenweise, mit Umbruch) |
| UP / DOWN halten | eine ganze Zeile hoch / runter |
| SELECT | Feld öffnen (auf einer Zahl: Nachbarn öffnen, wenn Fähnchen passen) |
| SELECT halten | Fähnchen setzen / entfernen |
| BACK | zurück ins Menü |

## Bauen

Benötigt das aktuelle Pebble SDK (`pebble-tool` 5.x, SDK 4.x):

```sh
uv tool install pebble-tool
pebble sdk install latest
pebble build
pebble install --emulator emery     # oder: pebble install --phone <IP>
```
