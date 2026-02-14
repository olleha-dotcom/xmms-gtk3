# GTK3 Porting Status

This tree has started a GTK3 migration with compatibility shims and low-risk API updates.

## Completed in this change set

- Added `gtk_compat.h` with GTK3 shims for legacy signal/object/timer/size APIs.
- Replaced direct `#include <gtk/gtk.h>` includes in source headers and C files with `#include "gtk_compat.h"`.
- Replaced legacy timer calls:
  - `gtk_timeout_add` -> `g_timeout_add`
  - `gtk_timeout_remove` -> `g_source_remove`
- Replaced legacy sizing and border calls:
  - `gtk_widget_set_usize` -> `gtk_widget_set_size_request`
  - `gtk_container_border_width` -> `gtk_container_set_border_width`
- Updated `configure.in` dependency checks to `pkg-config` with `glib-2.0`, `gthread-2.0`, and `gtk+-3.0`.

## Major blockers remaining

These APIs/widgets are removed in GTK3 and still need targeted rewrites:

- `GtkItemFactory` menu construction paths
- `GtkCList` usage in playlist/equalizer related windows
- `GtkCombo` and related direct struct field access
- `GtkFileSelection` dialogs
- GDK drawing and pixmap/font APIs:
  - `GdkPixmap`, `gdk_draw_pixmap`, `gdk_font_*`, `gdk_text_width*`
- `gtk_type_*` object/type APIs in `libxmms/xentry.c`
- Legacy `GtkObject` data APIs where behavior differs under `GObject`

## Next recommended implementation batch

1. Replace `GtkItemFactory` with `GtkMenu`/`GtkAction` (or a custom menu builder).
2. Port `GtkFileSelection` call sites to `GtkFileChooserDialog`.
3. Port `GtkCombo` + `GtkCList` call sites to `GtkComboBoxText`/`GtkTreeView`.
4. Move custom drawing to Cairo/Pango on `GtkDrawingArea`.
5. Port custom `xentry` type to `G_DEFINE_TYPE`.
