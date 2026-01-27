# WLED Usermod: Sequential Unlock

Ein einfaches Usermod für [WLED](https://github.com/Aircoookie/WLED), das beim Ein- und Ausschalten eine LED-für-LED-Animation abspielt.

## Features

- Separate Animation für Einschalten und Ausschalten (Ausschlaten ist noch in arbeit) Segment Transition auf min 3 sec stellen
- Konfigurierbare Verzögerung pro LED (5–10000 ms)
- Steuerbar über WebUI und API

## API Beispiel

http://<WLED-IP>/sequentialunlock?up=1&down=0&delay=50


## Installation

1. Kopiere `usermod_sequential_unlock.h` nach `usermods/` in dein WLED-Projekt.
2. Baue WLED mit aktiviertem Usermod.


MIT License

Copyright (c) 2025 L0rz

Permission is hereby granted, free of charge, to any person obtaining a copy...
