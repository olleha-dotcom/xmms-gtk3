#include "pulse.h"

#include <string.h>
#include <pulse/pulseaudio.h>
#include <pulse/timeval.h>

/* All stream/control state is protected by the Pulse mainloop lock. */
static pa_threaded_mainloop *pulse_loop;
static pa_context *pulse_context;
static pa_stream *pulse_stream;
static pa_operation *pulse_drain;
static gboolean pulse_paused, pulse_failed, pulse_drained = TRUE, pulse_flushing;
static gboolean pulse_opening;
static guint64 pulse_written_bytes, pulse_output_time_offset;
static gint pulse_last_time;
static pa_sample_spec pulse_spec;
static gint pulse_channels = 2;
static pa_sample_format_t pulse_format = PA_SAMPLE_S16NE;
static gboolean pulse_convert_s8_to_u8, pulse_convert_u16_to_s16;
static gint pulse_soft_gain_left = 100, pulse_soft_gain_right = 100;
static gint pulse_balance_left = 100, pulse_balance_right = 100;
static gint pulse_requested_left = 100, pulse_requested_right = 100;
static gint pulse_system_volume = -1, pulse_pending_volume = -1, pulse_sent_volume;
static gboolean pulse_volume_inflight, pulse_software_volume;
static pa_cvolume pulse_sink_volume;
static gchar *pulse_sink_name;
static guint pulse_save_source, pulse_reconnect_source;

static void pulse_refresh_sink(void);
static void pulse_apply_pending_volume(void);

static void pulse_signal_stream(pa_stream *stream, void *data)
{
	pa_threaded_mainloop_signal(pulse_loop, 0);
}

static void pulse_write_ready(pa_stream *stream, size_t length, void *data)
{
	pa_threaded_mainloop_signal(pulse_loop, 0);
}

typedef struct
{
	gboolean expired;
	pa_time_event *timer;
} PulseWait;

static void pulse_wait_timeout(pa_mainloop_api *api, pa_time_event *event,
		const struct timeval *tv, void *data)
{
	((PulseWait *) data)->expired = TRUE;
	pa_threaded_mainloop_signal(pulse_loop, 0);
}

static void pulse_wait_begin(PulseWait *wait)
{
	struct timeval deadline;
	pa_mainloop_api *api = pa_threaded_mainloop_get_api(pulse_loop);
	wait->expired = FALSE;
	pa_timeval_add(pa_gettimeofday(&deadline), 5 * PA_USEC_PER_SEC);
	wait->timer = api->time_new(api, &deadline, pulse_wait_timeout, wait);
	if (!wait->timer)
		wait->expired = TRUE;
}

static void pulse_wait_end(PulseWait *wait)
{
	if (wait->timer)
		pa_threaded_mainloop_get_api(pulse_loop)->time_free(wait->timer);
}

static gboolean pulse_ready(void)
{
	return pulse_stream && !pulse_failed &&
		pa_context_get_state(pulse_context) == PA_CONTEXT_READY &&
		pa_stream_get_state(pulse_stream) == PA_STREAM_READY;
}

static void pulse_stream_success(pa_stream *stream, int success, void *data)
{
	if (data)
		*(gint *) data = success;
	else if (stream == pulse_stream && !success)
		pulse_failed = TRUE;
	pa_threaded_mainloop_signal(pulse_loop, 0);
}

/* Setup/seek acknowledgements have deadlines; volume queries never wait here. */
static gboolean pulse_wait_operation(pa_operation *operation, gint *success)
{
	PulseWait wait;
	gboolean ok;
	if (!operation)
		return FALSE;
	pulse_wait_begin(&wait);
	while (!wait.expired && pulse_ready() &&
	       pa_operation_get_state(operation) == PA_OPERATION_RUNNING)
		pa_threaded_mainloop_wait(pulse_loop);
	ok = !wait.expired && *success &&
		pa_operation_get_state(operation) == PA_OPERATION_DONE;
	pa_operation_cancel(operation);
	pa_operation_unref(operation);
	pulse_wait_end(&wait);
	return ok;
}

static void pulse_update_gains(void)
{
	pulse_soft_gain_left = pulse_software_volume ? pulse_requested_left : pulse_balance_left;
	pulse_soft_gain_right = pulse_software_volume ? pulse_requested_right : pulse_balance_right;
}

static void pulse_volume_set_done(pa_context *context, int success, void *data)
{
	if (context != pulse_context)
		return;
	pulse_volume_inflight = FALSE;
	pulse_software_volume = !success;
	if (success && pulse_pending_volume == pulse_sent_volume)
	{
		pulse_system_volume = pulse_sent_volume;
		pulse_pending_volume = -1;
	}
	pulse_update_gains();
	if (success)
		pulse_apply_pending_volume();
}

static void pulse_apply_pending_volume(void)
{
	pa_operation *op;
	pa_cvolume volume;
	if (pulse_pending_volume < 0 || pulse_volume_inflight || !pulse_sink_name ||
	    !pulse_context || pa_context_get_state(pulse_context) != PA_CONTEXT_READY ||
	    !pa_cvolume_valid(&pulse_sink_volume))
		return;
	pa_cvolume_set(&volume, pulse_sink_volume.channels,
		(pa_volume_t) ((guint64) pulse_pending_volume * PA_VOLUME_NORM / 100));
	pulse_sent_volume = pulse_pending_volume;
	op = pa_context_set_sink_volume_by_name(pulse_context, pulse_sink_name,
		&volume, pulse_volume_set_done, NULL);
	if (op)
	{
		pulse_volume_inflight = TRUE;
		pa_operation_unref(op);
	}
	else
	{
		pulse_software_volume = TRUE;
		pulse_update_gains();
	}
}

static void pulse_sink_info(pa_context *context, const pa_sink_info *info,
		int eol, void *data)
{
	if (context != pulse_context || !info || eol)
		return;
	g_free(pulse_sink_name);
	pulse_sink_name = g_strdup(info->name);
	pulse_sink_volume = info->volume;
	if (pulse_pending_volume < 0 && !pulse_volume_inflight)
		pulse_system_volume = CLAMP((gint) ((guint64) pa_cvolume_avg(&info->volume) *
			100 / PA_VOLUME_NORM), 0, 100);
	pulse_apply_pending_volume();
}

static void pulse_request_sink(const char *name)
{
	pa_operation *op;
	if (!name)
		return;
	op = pa_context_get_sink_info_by_name(pulse_context, name, pulse_sink_info, NULL);
	if (op)
		pa_operation_unref(op);
}

static void pulse_server_info(pa_context *context, const pa_server_info *info, void *data)
{
	if (context == pulse_context && info)
		pulse_request_sink(info->default_sink_name);
}

static void pulse_refresh_sink(void)
{
	pa_operation *op;
	if (!pulse_context || pa_context_get_state(pulse_context) != PA_CONTEXT_READY)
		return;
	if (pulse_ready())
		pulse_request_sink(pa_stream_get_device_name(pulse_stream));
	else if (pulse_cfg.device && *pulse_cfg.device)
		pulse_request_sink(pulse_cfg.device);
	else
	{
		op = pa_context_get_server_info(pulse_context, pulse_server_info, NULL);
		if (op)
			pa_operation_unref(op);
	}
}

static void pulse_subscription(pa_context *context, pa_subscription_event_type_t type,
		uint32_t index, void *data)
{
	if ((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK ||
	    (type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SERVER)
		pulse_refresh_sink();
}

static void pulse_stream_moved(pa_stream *stream, void *data)
{
	if (stream == pulse_stream)
		pulse_refresh_sink();
}

static void pulse_context_state(pa_context *context, void *data)
{
	pa_operation *op;
	if (pa_context_get_state(context) == PA_CONTEXT_READY)
	{
		pa_context_set_subscribe_callback(context, pulse_subscription, NULL);
		op = pa_context_subscribe(context,
			PA_SUBSCRIPTION_MASK_SINK | PA_SUBSCRIPTION_MASK_SERVER, NULL, NULL);
		if (op)
			pa_operation_unref(op);
		pulse_refresh_sink();
	}
	pa_threaded_mainloop_signal(pulse_loop, 0);
}

static void pulse_cancel_drain(void)
{
	if (pulse_drain)
	{
		pa_operation_cancel(pulse_drain);
		pa_operation_unref(pulse_drain);
		pulse_drain = NULL;
	}
}

static void pulse_disconnect_stream(void)
{
	pulse_cancel_drain();
	if (pulse_stream)
	{
		pa_stream_set_state_callback(pulse_stream, NULL, NULL);
		pa_stream_set_write_callback(pulse_stream, NULL, NULL);
		pa_stream_set_moved_callback(pulse_stream, NULL, NULL);
		pa_stream_disconnect(pulse_stream);
		pa_stream_unref(pulse_stream);
		pulse_stream = NULL;
	}
	pulse_written_bytes = pulse_output_time_offset = 0;
	pulse_last_time = 0;
	pulse_paused = pulse_flushing = FALSE;
	pulse_drained = TRUE;
}

static void pulse_connect_context(void)
{
	if (pulse_context)
	{
		if (pulse_stream)
			pulse_failed = TRUE;
		pulse_disconnect_stream();
		pa_context_set_state_callback(pulse_context, NULL, NULL);
		pa_context_disconnect(pulse_context);
		pa_context_unref(pulse_context);
	}
	pulse_volume_inflight = FALSE;
	pulse_system_volume = -1;
	pa_cvolume_init(&pulse_sink_volume);
	g_clear_pointer(&pulse_sink_name, g_free);
	pulse_context = pa_context_new(pa_threaded_mainloop_get_api(pulse_loop), "XMMS");
	if (!pulse_context)
		return;
	pa_context_set_state_callback(pulse_context, pulse_context_state, NULL);
	pa_context_connect(pulse_context,
		(pulse_cfg.server && *pulse_cfg.server) ? pulse_cfg.server : NULL,
		PA_CONTEXT_NOFLAGS, NULL);
}

static gboolean pulse_reconnect(gpointer data)
{
	if (pulse_loop)
	{
		pa_threaded_mainloop_lock(pulse_loop);
		if (!pulse_opening &&
		    (!pulse_context || !PA_CONTEXT_IS_GOOD(pa_context_get_state(pulse_context))))
			pulse_connect_context();
		pa_threaded_mainloop_unlock(pulse_loop);
	}
	return G_SOURCE_CONTINUE;
}

void pulse_start(void)
{
	gint base = MAX(pulse_cfg.volume_left, pulse_cfg.volume_right);
	if (pulse_loop)
		return;
	pulse_requested_left = pulse_cfg.volume_left;
	pulse_requested_right = pulse_cfg.volume_right;
	pulse_balance_left = base ? pulse_requested_left * 100 / base : 100;
	pulse_balance_right = base ? pulse_requested_right * 100 / base : 100;
	pulse_update_gains();
	pulse_loop = pa_threaded_mainloop_new();
	if (!pulse_loop)
		return;
	if (pa_threaded_mainloop_start(pulse_loop) < 0)
	{
		pa_threaded_mainloop_free(pulse_loop);
		pulse_loop = NULL;
		return;
	}
	pa_threaded_mainloop_lock(pulse_loop);
	pulse_connect_context();
	pa_threaded_mainloop_unlock(pulse_loop);
	pulse_reconnect_source = g_timeout_add_seconds(2, pulse_reconnect, NULL);
}

static gboolean pulse_save_later(gpointer data)
{
	pulse_save_source = 0;
	pulse_save_config();
	return G_SOURCE_REMOVE;
}

void pulse_shutdown(void)
{
	if (pulse_save_source)
	{
		g_source_remove(pulse_save_source);
		pulse_save_source = 0;
		pulse_save_config();
	}
	if (pulse_reconnect_source)
	{
		g_source_remove(pulse_reconnect_source);
		pulse_reconnect_source = 0;
	}
	if (!pulse_loop)
		return;
	pa_threaded_mainloop_lock(pulse_loop);
	pulse_disconnect_stream();
	if (pulse_context)
	{
		pa_context_set_state_callback(pulse_context, NULL, NULL);
		pa_context_disconnect(pulse_context);
		pa_context_unref(pulse_context);
		pulse_context = NULL;
	}
	g_clear_pointer(&pulse_sink_name, g_free);
	pa_threaded_mainloop_unlock(pulse_loop);
	pa_threaded_mainloop_stop(pulse_loop);
	pa_threaded_mainloop_free(pulse_loop);
	pulse_loop = NULL;
}

/* The legacy OutputPlugin ABI has no cleanup hook; dlclose must stop callbacks. */
#if defined(__GNUC__)
static void __attribute__((destructor)) pulse_unload(void)
{
	pulse_shutdown();
}
#endif
static gboolean pulse_map_format(AFormat fmt, pa_sample_format_t *out_fmt)
{
	pulse_convert_s8_to_u8 = FALSE;
	pulse_convert_u16_to_s16 = FALSE;

	switch (fmt)
	{
	case FMT_U8:
		*out_fmt = PA_SAMPLE_U8;
		return TRUE;
	case FMT_S8:
		*out_fmt = PA_SAMPLE_U8;
		pulse_convert_s8_to_u8 = TRUE;
		return TRUE;
	case FMT_S16_LE:
		*out_fmt = PA_SAMPLE_S16LE;
		return TRUE;
	case FMT_S16_BE:
		*out_fmt = PA_SAMPLE_S16BE;
		return TRUE;
	case FMT_S16_NE:
		*out_fmt = PA_SAMPLE_S16NE;
		return TRUE;
	case FMT_U16_LE:
		*out_fmt = PA_SAMPLE_S16LE;
		pulse_convert_u16_to_s16 = TRUE;
		return TRUE;
	case FMT_U16_BE:
		*out_fmt = PA_SAMPLE_S16BE;
		pulse_convert_u16_to_s16 = TRUE;
		return TRUE;
	case FMT_U16_NE:
		*out_fmt = PA_SAMPLE_S16NE;
		pulse_convert_u16_to_s16 = TRUE;
		return TRUE;
	default:
		return FALSE;
	}
}

static guint16 pulse_read_u16(const guchar *p)
{
	guint16 v;
	memcpy(&v, p, sizeof(v));
	if (pulse_format == PA_SAMPLE_S16BE)
		return GUINT16_FROM_BE(v);
	return GUINT16_FROM_LE(v);
}

static void pulse_write_u16(guchar *p, guint16 v)
{
	if (pulse_format == PA_SAMPLE_S16BE)
		v = GUINT16_TO_BE(v);
	else
		v = GUINT16_TO_LE(v);
	memcpy(p, &v, sizeof(v));
}

static void pulse_apply_volume_u8(guchar *data, gint length)
{
	gint i;
	gint mono = (pulse_soft_gain_left + pulse_soft_gain_right) / 2;
	for (i = 0; i < length; i++)
	{
		gint gain = mono;
		gint ch = (pulse_channels > 0) ? (i % pulse_channels) : 0;
		gint s;
		if (pulse_channels >= 2)
		{
			if (ch == 0)
				gain = pulse_soft_gain_left;
			else if (ch == 1)
				gain = pulse_soft_gain_right;
		}
		s = ((gint) data[i]) - 128;
		s = (s * gain) / 100;
		s = CLAMP(s + 128, 0, 255);
		data[i] = (guchar) s;
	}
}

static void pulse_apply_volume_s16(guchar *data, gint length)
{
	gint i;
	gint mono = (pulse_soft_gain_left + pulse_soft_gain_right) / 2;
	for (i = 0; i + 1 < length; i += 2)
	{
		gint gain = mono;
		gint ch = (pulse_channels > 0) ? ((i / 2) % pulse_channels) : 0;
		gint32 s;
		guint16 raw = pulse_read_u16(data + i);

		if (pulse_channels >= 2)
		{
			if (ch == 0)
				gain = pulse_soft_gain_left;
			else if (ch == 1)
				gain = pulse_soft_gain_right;
		}

		s = (gint16) raw;
		s = (s * gain) / 100;
		s = CLAMP(s, -32768, 32767);
		pulse_write_u16(data + i, (guint16) ((gint16) s));
	}
}

static void pulse_transform_buffer(guchar *data, gint length)
{
	gint i;

	if (pulse_convert_s8_to_u8)
	{
		for (i = 0; i < length; i++)
			data[i] ^= 0x80;
	}
	if (pulse_convert_u16_to_s16)
	{
		for (i = 0; i + 1 < length; i += 2)
		{
			guint16 v = pulse_read_u16(data + i);
			v ^= 0x8000;
			pulse_write_u16(data + i, v);
		}
	}

	if (pulse_soft_gain_left == 100 && pulse_soft_gain_right == 100)
		return;

	if (pulse_format == PA_SAMPLE_U8)
		pulse_apply_volume_u8(data, length);
	else
		pulse_apply_volume_s16(data, length);
}

static gint pulse_written_time_locked(void)
{
	guint64 time = pulse_output_time_offset +
		pa_bytes_to_usec(pulse_written_bytes, &pulse_spec) / PA_USEC_PER_MSEC;
	return (gint) MIN(time, G_MAXINT);
}

static gint pulse_output_time_locked(void)
{
	pa_usec_t latency;
	int negative = 0;
	gint written = pulse_written_time_locked();
	gint64 played;
	if (pulse_paused || pulse_flushing)
		return pulse_last_time;
	if (pulse_drained)
		pulse_last_time = written;
	else if (pa_stream_get_latency(pulse_stream, &latency, &negative) >= 0)
	{
		played = written - (negative ? 0 : (gint64) (latency / PA_USEC_PER_MSEC));
		pulse_last_time = CLAMP(played, pulse_last_time, written);
	}
	return pulse_last_time;
}

int pulse_open(AFormat fmt, int rate, int nch)
{
	PulseWait wait;
	pa_buffer_attr attr;
	gboolean ok = FALSE;
	if (!pulse_loop)
		return 0;
	pa_threaded_mainloop_lock(pulse_loop);
	pulse_disconnect_stream();
	pulse_failed = FALSE;
	pulse_opening = TRUE;
	if (!pulse_map_format(fmt, &pulse_format) || rate <= 0 || nch < 1 || nch > PA_CHANNELS_MAX)
		goto out;
	pulse_spec.format = pulse_format;
	pulse_spec.rate = rate;
	pulse_spec.channels = nch;
	pulse_channels = nch;
	if (!pa_sample_spec_valid(&pulse_spec))
		goto out;
	if (!pulse_context || !PA_CONTEXT_IS_GOOD(pa_context_get_state(pulse_context)))
		pulse_connect_context();
	if (!pulse_context)
		goto out;
	pulse_wait_begin(&wait);
	while (!wait.expired && PA_CONTEXT_IS_GOOD(pa_context_get_state(pulse_context)) &&
	       pa_context_get_state(pulse_context) != PA_CONTEXT_READY)
		pa_threaded_mainloop_wait(pulse_loop);
	ok = !wait.expired && pa_context_get_state(pulse_context) == PA_CONTEXT_READY;
	pulse_wait_end(&wait);
	if (!ok)
		goto out;
	ok = FALSE;
	pulse_stream = pa_stream_new(pulse_context,
		(pulse_cfg.stream_name && *pulse_cfg.stream_name) ? pulse_cfg.stream_name : "XMMS",
		&pulse_spec, NULL);
	if (!pulse_stream)
		goto out;
	pa_stream_set_state_callback(pulse_stream, pulse_signal_stream, NULL);
	pa_stream_set_write_callback(pulse_stream, pulse_write_ready, NULL);
	pa_stream_set_moved_callback(pulse_stream, pulse_stream_moved, NULL);
	attr.maxlength = pa_usec_to_bytes(2 * PA_USEC_PER_SEC, &pulse_spec);
	attr.tlength = pa_usec_to_bytes(250 * PA_USEC_PER_MSEC, &pulse_spec);
	/* Automatic prebuffering stops the read index running past an underrun. */
	attr.prebuf = (uint32_t) -1;
	attr.minreq = (uint32_t) -1;
	attr.fragsize = (uint32_t) -1;
	if (pa_stream_connect_playback(pulse_stream,
		(pulse_cfg.device && *pulse_cfg.device) ? pulse_cfg.device : NULL, &attr,
		PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_AUTO_TIMING_UPDATE | PA_STREAM_ADJUST_LATENCY,
		NULL, NULL) < 0)
		goto out;
	pulse_wait_begin(&wait);
	while (!wait.expired && PA_CONTEXT_IS_GOOD(pa_context_get_state(pulse_context)) &&
	       PA_STREAM_IS_GOOD(pa_stream_get_state(pulse_stream)) &&
	       pa_stream_get_state(pulse_stream) != PA_STREAM_READY)
		pa_threaded_mainloop_wait(pulse_loop);
	ok = !wait.expired && pulse_ready();
	pulse_wait_end(&wait);
	if (ok)
		pulse_refresh_sink();
out:
	pulse_opening = FALSE;
	if (!ok)
	{
		gboolean creating = pulse_stream &&
			pa_stream_get_state(pulse_stream) == PA_STREAM_CREATING;
		g_warning("pulse_open(): %s", pulse_context ?
			pa_strerror(pa_context_errno(pulse_context)) : "Cannot create PulseAudio context");
		pulse_disconnect_stream();
		/* A timed-out creation has no channel to disconnect yet. */
		if (creating)
			pa_context_disconnect(pulse_context);
		pulse_failed = TRUE;
	}
	pa_threaded_mainloop_unlock(pulse_loop);
	return ok;
}

void pulse_write(void *ptr, int length)
{
	guchar *copy = NULL;
	if (!pulse_loop || !ptr || length <= 0)
		return;
	pa_threaded_mainloop_lock(pulse_loop);
	if (!pulse_ready() || pulse_flushing)
		goto out;
	if (length % pa_frame_size(&pulse_spec))
	{
		pulse_failed = TRUE;
		goto out;
	}
	if (pulse_convert_s8_to_u8 || pulse_convert_u16_to_s16 ||
	    pulse_soft_gain_left != 100 || pulse_soft_gain_right != 100)
	{
		copy = g_malloc(length);
		memcpy(copy, ptr, length);
		pulse_transform_buffer(copy, length);
		ptr = copy;
	}
	pulse_cancel_drain();
	if (pa_stream_write(pulse_stream, ptr, length, NULL, 0, PA_SEEK_RELATIVE) < 0)
		pulse_failed = TRUE;
	else
	{
		pulse_written_bytes += length;
		pulse_drained = FALSE;
	}
out:
	g_free(copy);
	pa_threaded_mainloop_unlock(pulse_loop);
}

void pulse_close(void)
{
	if (!pulse_loop)
		return;
	pa_threaded_mainloop_lock(pulse_loop);
	/* EOF has already drained. Stop/Next must discard queued sound immediately. */
	pulse_disconnect_stream();
	pa_threaded_mainloop_unlock(pulse_loop);
}

void pulse_flush(int time)
{
	gint success = 0;
	gboolean ok = FALSE;
	if (!pulse_loop)
		return;
	pa_threaded_mainloop_lock(pulse_loop);
	pulse_cancel_drain();
	pulse_flushing = TRUE;
	pulse_written_bytes = 0;
	pulse_output_time_offset = MAX(0, time);
	pulse_last_time = MAX(0, time);
	pulse_drained = TRUE;
	if (pulse_ready())
	{
		ok = pulse_wait_operation(pa_stream_flush(pulse_stream,
			pulse_stream_success, &success), &success);
		if (ok)
		{
			success = 0;
			ok = pulse_wait_operation(pa_stream_update_timing_info(pulse_stream,
				pulse_stream_success, &success), &success);
		}
	}
	pulse_failed = !ok;
	pulse_flushing = FALSE;
	pa_threaded_mainloop_unlock(pulse_loop);
}

void pulse_pause(short paused)
{
	pa_operation *op;
	if (!pulse_loop)
		return;
	pa_threaded_mainloop_lock(pulse_loop);
	if (pulse_ready() && pulse_paused != !!paused)
	{
		pulse_output_time_locked();
		pulse_paused = !!paused;
		if (pulse_paused)
			pulse_cancel_drain();
		op = pa_stream_cork(pulse_stream, pulse_paused, pulse_stream_success, NULL);
		if (op)
			pa_operation_unref(op);
		else
			pulse_failed = TRUE;
	}
	pa_threaded_mainloop_unlock(pulse_loop);
}

int pulse_free(void)
{
	gint available = 0;
	size_t size;
	if (!pulse_loop)
		return G_MAXINT;
	pa_threaded_mainloop_lock(pulse_loop);
	if (!pulse_ready())
		available = G_MAXINT; /* Let the decoder unwind; output_time reports the error. */
	else if (!pulse_paused && !pulse_flushing)
	{
		size = pa_stream_writable_size(pulse_stream);
		if (size == (size_t) -1)
			pulse_failed = TRUE;
		else if (size > 0)
		{
			const pa_buffer_attr *attr = pa_stream_get_buffer_attr(pulse_stream);
			/* Decoders poll for a whole block, which can exceed tlength at
			 * low sample rates. Allow that block within the hard buffer limit. */
			if (attr && attr->maxlength > attr->tlength)
				size = MIN((guint64) attr->maxlength,
					(guint64) size + attr->maxlength - attr->tlength);
			available = MIN(size, G_MAXINT);
		}
	}
	pa_threaded_mainloop_unlock(pulse_loop);
	return available;
}

static void pulse_drain_done(pa_stream *stream, int success, void *data)
{
	if (stream == pulse_stream)
	{
		pulse_drained = !!success;
		pulse_failed = !success;
		if (success)
			pulse_last_time = pulse_written_time_locked();
	}
}

int pulse_playing(void)
{
	gboolean playing = FALSE;
	if (!pulse_loop)
		return FALSE;
	pa_threaded_mainloop_lock(pulse_loop);
	if (pulse_ready() && !pulse_flushing && !pulse_drained)
	{
		playing = TRUE;
		if (!pulse_paused && !pulse_drain)
		{
			pulse_drain = pa_stream_drain(pulse_stream, pulse_drain_done, NULL);
			if (!pulse_drain)
			{
				pulse_failed = TRUE;
				playing = FALSE;
			}
		}
	}
	pa_threaded_mainloop_unlock(pulse_loop);
	return playing;
}

int pulse_get_written_time(void)
{
	gint time = 0;
	if (!pulse_loop)
		return 0;
	pa_threaded_mainloop_lock(pulse_loop);
	if (pulse_stream)
		time = pulse_written_time_locked();
	pa_threaded_mainloop_unlock(pulse_loop);
	return time;
}

int pulse_get_output_time(void)
{
	gint time = 0;
	if (!pulse_loop)
		return 0;
	pa_threaded_mainloop_lock(pulse_loop);
	if (pulse_opening)
		time = 0;
	else if (pulse_failed || (pulse_stream && !pulse_ready()))
		time = -2;
	else if (pulse_stream)
		time = pulse_output_time_locked();
	pa_threaded_mainloop_unlock(pulse_loop);
	return time;
}

void pulse_get_volume(int *left, int *right)
{
	gint base;
	if (!pulse_loop)
	{
		if (left) *left = pulse_cfg.volume_left;
		if (right) *right = pulse_cfg.volume_right;
		return;
	}
	pa_threaded_mainloop_lock(pulse_loop);
	base = pulse_pending_volume >= 0 ? pulse_pending_volume : pulse_system_volume;
	if (base >= 0 && !pulse_software_volume)
	{
		if (left) *left = (base * pulse_balance_left + 50) / 100;
		if (right) *right = (base * pulse_balance_right + 50) / 100;
	}
	else
	{
		if (left) *left = pulse_requested_left;
		if (right) *right = pulse_requested_right;
	}
	pa_threaded_mainloop_unlock(pulse_loop);
}

void pulse_set_volume(int left, int right)
{
	gint base;
	left = CLAMP(left, 0, 100);
	right = CLAMP(right, 0, 100);
	if (pulse_loop)
	{
		pa_threaded_mainloop_lock(pulse_loop);
		base = MAX(left, right);
		pulse_requested_left = left;
		pulse_requested_right = right;
		pulse_balance_left = base ? (left * 100 + base / 2) / base : 100;
		pulse_balance_right = base ? (right * 100 + base / 2) / base : 100;
		pulse_pending_volume = base;
		pulse_software_volume = pulse_system_volume < 0;
		pulse_update_gains();
		pulse_apply_pending_volume();
		pa_threaded_mainloop_unlock(pulse_loop);
	}
	pulse_cfg.volume_left = left;
	pulse_cfg.volume_right = right;
	if (pulse_save_source)
		g_source_remove(pulse_save_source);
	pulse_save_source = g_timeout_add(500, pulse_save_later, NULL);
}
