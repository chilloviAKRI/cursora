---
name: cursora
description: Inspect and change installed Xcursor themes and cursor size on Omarchy with Cursora.
---

Use `cursora list --json` for installed themes and `cursora status --json` for saved settings.
Preview changes with `cursora apply --theme NAME --size 32 --dry-run --json`.
Apply an authorized choice using `cursora apply --theme NAME --size 32 --json` inside Hyprland.
Use `cursora undo --json` to restore the previous settings. Undo refuses to overwrite files edited since applying.
The command updates user Hyprland Lua, GTK, UWSM and default Xcursor settings, makes backups under `$XDG_STATE_HOME/cursora/backups`, validates the compositor configuration and applies the cursor live. Defaults follow the XDG Base Directory specification.
Use `cursora open` for the desktop interface.
