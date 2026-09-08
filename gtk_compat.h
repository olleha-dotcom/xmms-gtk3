#ifndef XMMS_GTK_COMPAT_H
#define XMMS_GTK_COMPAT_H

/* Keep this umbrella for legacy callers, including out-of-tree plugins.
 * The shims remain header-only so plugins need no extra link dependency. */
#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(3, 0, 0)
#include "gtk_compat/types.h"
#include "gtk_compat/signals.h"
#include "gtk_compat/drawing.h"
#include "gtk_compat/widgets.h"
#include "gtk_compat/menus.h"
#endif

#endif /* XMMS_GTK_COMPAT_H */
