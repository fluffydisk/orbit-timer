# Orbit Timer 3010

Orbit Timer is an offline Linux countdown with a circular progress dial and its own desktop interface. Start, pause, resume, reset, or pick a 1, 5, or 25 minute preset. The segmented dial drains as time passes, orbit markers move with elapsed time, and an amber alert marks zero alongside a desktop bell.

Download the x86-64 packages from [GitHub Releases](https://github.com/fluffydisk/orbit-timer/releases/latest).

### Ubuntu 22.04 or newer

```bash
curl -fLO https://github.com/fluffydisk/orbit-timer/releases/download/v1.2.0/orbit-timer_1.2.0_amd64.deb &&
sudo apt install ./orbit-timer_1.2.0_amd64.deb
```

### Omarchy / Arch Linux

```bash
curl -fLO https://github.com/fluffydisk/orbit-timer/releases/download/v1.2.0/orbit-timer-1.2.0-1-x86_64.pkg.tar.zst &&
sudo pacman -U ./orbit-timer-1.2.0-1-x86_64.pkg.tar.zst
```

- Standalone Linux x86-64: `chmod +x orbit-timer-x86_64 && ./orbit-timer-x86_64` (requires X11 and Cairo libraries).

Launch **Orbit Timer** from the app menu or run `orbit-timer`. On Wayland the app uses XWayland. Space starts and pauses, R resets, Up/Down adjust minutes, Left/Right adjust seconds, and Esc closes the window.

## Controls

- Set a custom duration with **Set duration** or **T**, then enter up to six digits as HHMMSS (for example, `013000` for 1 hour 30 minutes).
- **+1 min** can extend an active countdown. **Repeat** starts another round when the countdown reaches zero.
- **Stopwatch** has hundredths of a second and records laps. Press **L** or click **Lap**. The three latest cumulative lap times remain visible.
- Switching between the timer and stopwatch preserves both sessions and lets them continue while hidden.
- **Sound** or **M** mutes the desktop bell. **Pin window** requests always-on-top from the window manager.

## Build

Install a C compiler, make, pkg-config, X11 headers and Cairo headers. Run `make check` to compile and test. Run `sh package.sh` on x86-64 Linux to prepare packages in `dist/`; packaging requires zstd and either dpkg-deb or binutils.
