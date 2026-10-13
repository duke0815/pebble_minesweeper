# Minesweeper für Pebble Time 2

Minesweeper-Klon für die **Pebble Time 2** (Plattform `emery`, 200×228, Farbe).
Läuft zusätzlich auf `basalt`, `diorite` und `flint`. Auf der Time 2 per Touch oder Tasten spielbar, auf den anderen Modellen per Tasten.

## Features

- 8×8 Spielfeld (64 Felder)
- 3 Schwierigkeitsgrade: **Leicht** (6 Bomben), **Mittel** (10), **Schwer** (14)
- Erster Klick ist immer sicher (inkl. Nachbarfelder)
- Start direkt in der Schwierigkeitsauswahl; Steuerungs-Übersicht im Menü unter „Mehr“
- Explosions-Animation bei Bombe (Blitz, Feuerball, Druckwelle, Trümmer, „BOOM!“)
- Sieges-Animation (Konfetti + Banner)
- Neuer Highscore: Pokal-Animation mit rotierenden Strahlen und Funkeln
- Highscore-Liste: Top 5 Zeiten (Sekunden) pro Schwierigkeitsgrad, dauerhaft gespeichert
- Einstellungen: **Display immer an** (Backlight dauerhaft)
- Sprache automatisch nach Uhr-Einstellung: Deutsch bei deutscher Uhr, sonst Englisch
- Nach jedem Spiel: Highscore-Liste (gespielte Stufe und neuer Eintrag markiert), danach *Nochmal* oder *Beenden* (zurück ins Hauptmenü)

## Steuerung

| Taste | Aktion |
|---|---|
| UP | Cursor horizontal nach rechts (am Zeilenende zurück an den Anfang) |
| DOWN | Cursor vertikal nach unten (am Spaltenende zurück nach oben) |
| UP / DOWN halten | Cursor läuft automatisch weiter |
| SELECT | Feld öffnen (auf einer Zahl: Nachbarn öffnen, wenn Fähnchen passen) |
| SELECT halten | Fähnchen setzen / entfernen |
| BACK | zurück ins Menü |

### Touch (nur Pebble Time 2)

| Geste | Aktion |
|---|---|
| Feld antippen | Feld öffnen |
| Feld halten | Fähnchen setzen / entfernen |
| Kopfzeile antippen (Lupe) | Zoom an / aus |
| Ziehen (im Zoom) | Ausschnitt verschieben |
| Highscore-Liste / Steuerung antippen | weiter / zurück |

Mehrfinger-Gesten (Pinch-Zoom) unterstützt die Pebble-Touch-API nicht; sie liefert nur einen Berührungspunkt.

## Bauen

Benötigt das aktuelle Pebble SDK (`pebble-tool` 5.x, SDK 4.x):

```sh
uv tool install pebble-tool
pebble sdk install latest
pebble build
pebble install --emulator emery     # oder: pebble install --phone <IP>
```

## Store

Texte und Grafiken für die Veröffentlichung liegen in [`store/`](store/STORE_LISTING.md).
