# wlcrosshair

(AI Built)

A ~300 line crosshair overlay for **KDE Plasma on Wayland**, built against
Qt6 Gui + KDE's `LayerShellQt`. No GTK, Qt Widgets module, QML,
XWayland tricks. One source file for one purpose.

It exists because the AUR `wlcrosshair` package places itself at `(0,0)`
instead of the screen center on KWin. See "Why" below for the actual fix,
not a workaround.

## What it does

- Loads a PNG (any size, transparent background, crosshair drawn in the
  middle) from `~/.config/wlcrosshair/crosshair.png`.
- Places it dead-center on your screen via `wlr-layer-shell`, in the
  `overlay` layer (above normal windows, above fullscreen-*windowed*
  games).
- Fully click-through and takes no keyboard focus — verified not to
  interfere with the app underneath, including in fullscreen.
- Run it again to hide it. Bind one hotkey, toggle forever.
- No PNG yet? Draws a small built-in placeholder cross so first run
  shows something.

## Why it's built this way

**Centering.** The wlr-layer-shell protocol does *not* guarantee that an
unanchored surface gets centered — that's a courtesy some compositors
extend, and KWin isn't consistent about it. The only centering the
protocol actually promises: anchor two opposite edges and set an explicit
pixel margin. So that's the only method this tool uses — anchor
top+left, `margin = (screen_size - image_size) / 2`.

**Click-through.** `Qt::WindowTransparentForInput` isn't a "this process
ignores its own events" flag — on the Wayland platform plugin it sets an
*empty input region on the surface itself*, at the protocol level. Input
never reaches this window at all, which is what makes it safe inside
fullscreen games: there's no window underneath to accidentally block a
click or eat a camera-look event, because the surface was never in the
input path to begin with. (This is the class of bug GTK version hit
— input-shape regions applied late/at the wrong lifecycle point, or lost
across a resize — and it's why this tool is a plain fixed-size window
that never resizes after creation.)

**Singleton / toggle.** A pidfile guarded by `flock()`, not "read a
pidfile, `kill(pid, 0)` to see if it's alive, then decide." That pattern
races: fire the hotkey twice quickly and two processes can both decide
they're the first one. `flock()` the kernel drops it
automatically on any exit — clean shutdown, crash, `kill -9`, doesn't
matter — so there's no separate "handle a stale lock file" code path to
get subtly wrong. An unlockable file is either genuinely held right now,
or it isn't.

**No GTK, no Widgets module.** `QRasterWindow` is a bare `QWindow` with a
software-rendered paint surface — no widget tree, style engine, or layout system, nothing running that this tool doesn't use. Combined with
`LayerShellQt` that keeps both the dependency graph and the binary small
while staying pure Qt.

## Dependencies (Arch)

```
sudo pacman -S qt6-base layer-shell-qt cmake
```

`layer-shell-qt` is KDE's official Qt binding for `wlr-layer-shell` (also
used by `latte-dock` and the SDDM Wayland greeter) and lives in `extra` repo.

## Build & install

```
makepkg -si
```

run from inside this directory. `pacman -Qi wlcrosshair` will show it
afterwards like any other package.

Or without touching pacman at all:

```
cmake -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

(make sure `~/.local/bin` is on your `PATH`).

## Usage

```
wlcrosshair                 # show (or hide, if already showing)
wlcrosshair --list-outputs  # find your monitor's name, e.g. DP-1
wlcrosshair --image ~/Pictures/dot.png
wlcrosshair --output DP-1 --scale 2 --offset-x -3 --offset-y 0
```

Everything above can instead live in `~/.config/wlcrosshair/config.ini`
(copy `config/config.ini.example`), so plain `wlcrosshair` just does the
right thing. CLI flags always win over the config file.

### Your PNG

Drop it at `~/.config/wlcrosshair/crosshair.png` — transparent
background, crosshair centered in the image, any resolution (16×16,
24×24, 128×128, doesn't matter, the window is sized to match it exactly
in logical pixels). If you keep several PNGs in that folder for
different games, point `--image` at the one you want, or set `image=` in
`config.ini`.

### Bind it to a key

**System Settings → Shortcuts → Custom Shortcuts → New → Global Shortcut
→ Command/URL**, command `wlcrosshair`.

## Known limitations (Wayland's, not this tool's)

- **True exclusive fullscreen** (a dedicated fullscreen surface, as
  opposed to borderless-windowed) won't show any overlay on top of it —
  that's Wayland's per-surface isolation model. Run games in
  borderless/"fullscreen window" mode, which is what most people already
  do and is what layer-shell overlays require in general (this applies
  equally to `gamescope`-style overlays, OBS's game capture, etc).
- HiDPI: the window is sized in logical pixels. On a scaled display,
  either pre-scale your PNG or pass `--scale` to compensate.
- Needs a compositor implementing `wlr-layer-shell` — KWin has since
  Plasma 5.27.

## Files

```
CMakeLists.txt              build definition
src/main.cpp                the entire tool
PKGBUILD                    local pacman package build
[PURGED] config/config.ini.example   copy to ~/.config/wlcrosshair/config.ini
```

config/config.ini.example
; Copy this to ~/.config/wlcrosshair/config.ini and adjust as needed.
; Every value here can also be overridden on the command line
; (--image, --scale, --output, --offset-x, --offset-y).

[General]
; Absolute path to your crosshair PNG. Transparent background, crosshair
; drawn dead-center in the image, any resolution (16x16, 24x24, 128x128,
; whatever). If left empty, wlcrosshair looks for
; ~/.config/wlcrosshair/crosshair.png, then the first *.png it finds in
; that folder, then falls back to a small built-in placeholder.
image=

; Scale factor applied to the image, e.g. 2.0 for a HiDPI screen where
; you want a 24x24 asset to render at 48x48.
scale=1.0

; Which output to show the crosshair on. Leave empty for the primary
; screen, or run `wlcrosshair --list-outputs` to see names like DP-1,
; HDMI-A-1, eDP-1.
output=

; Fine-tune pixel offsets from dead-center, if you ever need them.
offset_x=0
offset_y=0



NOTES


TLDR; Run "wlcrosshair" to toggle, a single process runs, no daemons, same process detaches from the terminal meaning it can be thus closed. The process will survive until it is manually killed or you run "wlcrosshair" again, which will toggle the overlay off and kill the only process for this tool, so no, nothing runs in between and when the overlay is not visible/present(In sense of broken/hung/duplicating processes) on the screen. In my opinion this is way more handy as it is just as lightweight, and made more smartly. So the tool can be started and stopped from terminal, krunner(if you have the native runner plugin to run executable commands enabled), as main and more non intrusive methods. Or can be made as a keybind(For which way of use it was originally designed as it involves running the same command to starting AND stopping the tool, so hitting one key enables and and hitting it again disables and kills the process). Hope it would also help someone out, because I didn't find any lightweight overlay I would wanna use and would work for me so I came up with this, and I like it.

Yet again - made for personal use.
