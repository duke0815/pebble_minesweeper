# Store-Eintrag – Minesweeper

Texte zum Einfügen ins Entwickler-Portal (Englisch).

---

## App name

Minesweeper

## Category

Games

## Short description (Untertitel / Kurzbeschreibung)

The classic mine-clearing puzzle for Pebble Time 2 – play by touch or buttons, zoom in, chase your best times.

## Description

Minesweeper, rebuilt from scratch for Pebble Time 2.

Clear an 8×8 minefield right on your wrist – with your finger or with the buttons.

TOUCH (Pebble Time 2)
• Tap a cell to open it, hold it to plant or remove a flag.
• Tap the top bar to zoom in for bigger cells, drag with one finger to move around, tap the top bar again to zoom out.

BUTTONS (all models)
• UP moves the cursor across, DOWN moves it down. Both wrap around the edge; hold to move continuously.
• SELECT opens a cell, hold SELECT to plant or remove a flag.
• Open a number again to clear all its neighbors once the matching flags are in place.

FEATURES
• Three difficulty levels: Easy (6 mines), Medium (10), Hard (14)
• Your first move is always safe
• Highscores: a top-5 list of your fastest times for every difficulty, saved on the watch
• A big BOOM when you hit a mine, confetti when you win, and an animated trophy for every new record
• "Backlight always on" option in Settings, for long games in the dark
• English and German, picked automatically from your watch language
• Zoom mode with one-finger panning on the Pebble Time 2 touchscreen
• Runs entirely on the watch, no phone connection or companion app needed

Built for the color display of Pebble Time 2, and also runs on Pebble Time, Pebble 2 and Pebble 2 Duo.

## Release notes (v1.0.0)

First release: 8×8 Minesweeper with touch controls and zoom on Pebble Time 2, button controls on all models, three difficulty levels, top-5 highscores per level, explosion, victory and record animations, and an always-on backlight option.

## Keywords / Tags

minesweeper, puzzle, game, mines, logic, classic, highscore

---

## Asset-Übersicht

| Feld im Portal | Datei |
|---|---|
| Large icon (144×144) | `graphics/icon_144x144.png` |
| Small icon (80×80) | `graphics/icon_80x80.png` |
| Banner (720×320, optional, pro Plattform – überall dasselbe hochladen) | `graphics/banner_720x320.png` |
| Screenshots Pebble Time 2 (emery, 200×228, max. 5) | `screenshots/emery/*.png` |
| Screenshots Pebble Time (basalt, 144×168) | `screenshots/basalt/*.png` |
| Screenshots Pebble 2 / 2 Duo (diorite/flint, 144×168, s/w) | `screenshots/diorite/*.png` |
| *(kein Store-Feld)* Promo-Bilder 1080×1080 für Website, README, Reddit/Discord | `graphics/promo_*.png` |

Die Screenshots stammen aus dem Pebble-Emulator. Highscores und Spielstände wurden dafür
mit einem Testbuild vorbereitet; Grafik und Texte sind die der echten App.
Die Promo-Bilder sind gerenderte Mockups (keine Fotos). Neu erzeugen mit:

```sh
NODE_PATH=$(npm root -g) node store/promo-src/render.js
```
