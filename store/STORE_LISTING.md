# Store-Eintrag – Minesweeper

Texte zum Einfügen ins Entwickler-Portal (Englisch).

---

## App name

Minesweeper

## Category

Games

## Short description (Untertitel / Kurzbeschreibung)

The classic mine-clearing puzzle for Pebble Time 2 – two buttons, no touch, real highscores.

## Description

Minesweeper, rebuilt from scratch for Pebble Time 2.

Clear an 8×8 minefield using nothing but the buttons on your watch. No touch, no fiddly cursor, no phone needed:

• UP moves the cursor across, DOWN moves it down. Both wrap around the edge.
• Hold UP or DOWN to move the cursor continuously.
• SELECT opens a cell. Press it on a number to open every neighbor once the matching flags are in place.
• Hold SELECT to plant or remove a flag.

FEATURES
• Three difficulty levels: Easy (6 mines), Medium (10), Hard (14)
• Your first move is always safe
• Highscores: a top-5 list of your fastest times for every difficulty, saved on the watch
• A big BOOM when you hit a mine, confetti when you win, and an animated trophy for every new record
• "Backlight always on" option in Settings, for long games in the dark
• English and German, picked automatically from your watch language
• Runs entirely on the watch, no phone connection or companion app needed

Built for the color display of Pebble Time 2, and also runs on Pebble Time, Pebble 2 and Pebble 2 Duo.

## Release notes (v1.0.0)

First release: 8×8 Minesweeper with three difficulty levels, top-5 highscores per level, explosion, victory and record animations, and an always-on backlight option.

## Keywords / Tags

minesweeper, puzzle, game, mines, logic, classic, highscore

---

## Asset-Übersicht

| Feld im Portal | Datei |
|---|---|
| Large icon (144×144) | `graphics/icon_144x144.png` |
| Small icon (48×48) | `graphics/icon_48x48.png` |
| Header/Banner (720×320) | `graphics/banner_720x320.png` |
| Screenshots Pebble Time 2 (emery, 200×228) | `screenshots/emery/*.png` |
| Screenshots Pebble Time (basalt, 144×168) | `screenshots/basalt/*.png` |
| Screenshots Pebble 2 / 2 Duo (diorite/flint, 144×168, s/w) | `screenshots/diorite/*.png` |
| Promo-Bilder (1080×1080, Website/Social) | `graphics/promo_*.png` |

Die Screenshots stammen aus dem Pebble-Emulator. Highscores und Spielstände wurden dafür
mit einem Testbuild vorbereitet; Grafik und Texte sind die der echten App.
Die Promo-Bilder sind gerenderte Mockups (keine Fotos). Neu erzeugen mit:

```sh
NODE_PATH=$(npm root -g) node store/promo-src/render.js
```
