# WLED Usermod: Sequential Unlock

Ein einfaches Usermod für [WLED](https://github.com/wled/WLED), das beim Ein- und Ausschalten eine LED-für-LED-Animation abspielt.

## Features

- Separate Animation für Ein- und Ausschalten
- Konfigurierbare Verzögerung pro LED (5–10000 ms)
- Steuerbar über WebUI (Usermod-Settings), JSON-State und HTTP-API

## HTTP-API

```
http://<WLED-IP>/sequentialunlock?up=1&down=0&delay=50
```

Alle Parameter sind optional; nur die übergebenen werden geändert. Die
Einstellungen werden persistent gespeichert (cfg.json).

| Parameter | Werte        | Bedeutung                                 |
|-----------|--------------|-------------------------------------------|
| `up`      | `0` / `1`    | Einschaltanimation aus / an               |
| `down`    | `0` / `1`    | Ausschaltanimation aus / an               |
| `delay`   | `5`–`10000`  | Verzögerung pro LED in ms                 |

Antwort (JSON) = aktueller Zustand, z. B.:

```json
{"up":true,"down":false,"delay":50}
```

Ein Aufruf ohne Parameter (`/sequentialunlock`) liefert die aktuelle
Konfiguration zurück, ohne etwas zu ändern.

## Installation

Im L0rz/WLED-Fork ist dieses Usermod bereits über den Build-Env
`[env:nodemcuv2_l0rz]` eingebunden (`custom_usermods = ... usermod_onoffanimation`).
Bauen mit:

```
pio run -e nodemcuv2_l0rz
```

In einem frischen WLED-Checkout: Ordner nach `usermods/` kopieren und den
Namen zu `custom_usermods` der gewünschten Build-Umgebung hinzufügen.

MIT License — Copyright (c) 2025 L0rz
