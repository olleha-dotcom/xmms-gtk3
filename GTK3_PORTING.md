# GTK3 Porting Status

The GTK3 port is functional and retains the classic skin and plugin interfaces.
Build and installation instructions are in README. VERSION is the upstream
version source; ChangeLog records maintenance changes.

## Current Architecture

- `gtk_compat.h` is the public umbrella for focused compatibility headers in
  `gtk_compat/`: types, signals, widgets, drawing, menus and Cairo helpers.
- Legacy lists and menus are backed by GTK3 widgets; file selection uses GTK3
  file chooser dialogs. Compatibility names remain for existing plugin sources.
- Main, equalizer and playlist windows render their skin surfaces through GTK3
  draw handlers. Cairo handles doublesize scaling with nearest-neighbor sampling;
  no extra doubled skin buffers are needed.
- `xmms/ui_scale.h` defines the shared logical-coordinate/skin-coordinate mapping.
  GDK monitor scaling and XMMS doublesize are distinct. Root drag coordinates must
  not be rescaled as skin coordinates. Playlist layout has its own sizing model.
- PulseAudio uses a persistent threaded context. GUI volume reads use cached
  subscription state; decoder setup and seek waits have finite deadlines.
- Decoder EOF is authoritative. `xmms/playback.h` provides a last-resort watchdog
  only for an unpaused position that stops advancing near a known track end.

## Regression Checks

Run `make check` and `make check-sanitizers` after a configured build. Tests cover
malformed BMP data, configuration write failures, Cairo scaling, playback/resume
policy and isolated PulseAudio stream operations. GUI tests use a private Xvfb
server, not the user's display. See README for dependencies and skipped tests.

## Remaining Migration Work

The compatibility layer is deliberately not a claim that every legacy API has
been eliminated. Plugins still use drawing/font adapters and deprecated GTK3
widgets. Replace those incrementally, with tests, rather than changing the
public plugin ABI wholesale. Native Wayland window placement and decorations
also need compositor-specific manual testing; Xvfb only verifies X11 behavior.
Legacy decoder open/seek entry points can still run synchronously from GUI
callbacks. PulseAudio waits are bounded, but making every decoder operation
asynchronous requires a separate input-plugin lifecycle change.
