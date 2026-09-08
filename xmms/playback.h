#ifndef XMMS_PLAYBACK_H
#define XMMS_PLAYBACK_H

#include <glib.h>

typedef struct
{
	guint generation;
	gint position_ms;
	gint64 progress_at_us;
	gboolean initialized;
	gboolean advanced;
} XmmsPlaybackMonitor;

/* Metadata is only a fallback. Healthy playback must always reach decoder EOF. */
static inline gboolean xmms_playback_stalled_at_end(XmmsPlaybackMonitor *monitor,
		guint generation, gint position_ms, gint length_ms,
		gboolean paused, gint64 now_us)
{
	if (!monitor->initialized || monitor->generation != generation ||
	    monitor->position_ms != position_ms || paused || position_ms <= 0 ||
	    length_ms <= 0 || now_us < monitor->progress_at_us)
	{
		monitor->generation = generation;
		monitor->position_ms = position_ms;
		monitor->progress_at_us = now_us;
		monitor->initialized = TRUE;
		monitor->advanced = FALSE;
		return FALSE;
	}
	if (!monitor->advanced && position_ms >= MAX(0, length_ms - 1500) &&
	    now_us - monitor->progress_at_us >= 3 * G_USEC_PER_SEC)
	{
		monitor->advanced = TRUE;
		return TRUE;
	}
	return FALSE;
}

static inline gboolean xmms_resume_should_start(gboolean enabled,
		gboolean was_playing, gint seconds, gboolean explicit_request,
		gint playlist_length)
{
	return enabled && was_playing && seconds > 0 &&
		!explicit_request && playlist_length > 0;
}

static inline gint xmms_resume_position(gint seconds, gint length_ms)
{
	if (length_ms > 0)
		return CLAMP(seconds, 0, MAX(0, (length_ms - 1) / 1000));
	return MAX(0, seconds);
}

#endif
