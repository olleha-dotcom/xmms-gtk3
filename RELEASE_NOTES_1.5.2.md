## XMMS GTK3 1.5.2

This release focuses on GTK3 usability fixes, visualization correctness, hi-res skin compatibility, and improved Ubuntu packaging.

### Highlights
- Fixed GTK3 menu/radio item handling so visualization mode selections update correctly.
- Added hi-res skin compatibility scaling for oversized skin bitmaps.
- Fixed equalizer hit-testing in doublesize mode (pointer mapping corrected).
- Wired shaded scope rendering modes properly (dot/line/solid).
- Improved end-of-track transition handling to avoid stalls on the final second.
- Fixed visualization stream recovery after seeking.
- Added clearer runtime EQ status feedback for unsupported/flat plugin states.

### Packaging and Desktop Integration
- Debian package now includes desktop integration:
  - `.desktop` launcher
  - application icon
  - desktop database hooks
- Version bumped to `1.5.2` in build/package metadata.

### Notes
- This is an incremental stability and UX release on top of the GTK3 port for modern Linux desktops (Ubuntu 24.04+).
