#include "pulse.h"

#include <string.h>
#include <pulse/pulseaudio.h>

static pa_simple *pulse_conn = NULL;
static gboolean pulse_paused = FALSE;
static guint64 pulse_written_bytes = 0;
static guint64 pulse_output_time_offset = 0;
static gint pulse_bps = 1;
static gint pulse_channels = 2;
static pa_sample_format_t pulse_format = PA_SAMPLE_S16NE;
static gboolean pulse_convert_s8_to_u8 = FALSE;
static gboolean pulse_convert_u16_to_s16 = FALSE;
static gint pulse_soft_gain_left = 100;
static gint pulse_soft_gain_right = 100;
static gint pulse_hw_volume = 100;
static gint pulse_cached_system_volume = -1;
static gint64 pulse_cached_system_volume_at = 0;
static gint pulse_cached_latency_ms = 0;
static gint64 pulse_cached_latency_at = 0;

typedef struct
{
	pa_mainloop *ml;
	pa_context *ctx;
	gboolean done;
	gboolean success;
	gchar *default_sink;
	pa_cvolume volume;
}
PulseCtl;

static void pulse_ctl_server_info_cb(pa_context *c, const pa_server_info *i, void *userdata)
{
	PulseCtl *ctl = (PulseCtl *) userdata;
	if (!i || !i->default_sink_name)
	{
		ctl->success = FALSE;
		ctl->done = TRUE;
		return;
	}
	ctl->default_sink = g_strdup(i->default_sink_name);
	ctl->success = TRUE;
	ctl->done = TRUE;
}

static void pulse_ctl_sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
	PulseCtl *ctl = (PulseCtl *) userdata;
	if (eol > 0)
	{
		if (!ctl->done)
		{
			ctl->success = FALSE;
			ctl->done = TRUE;
		}
		return;
	}
	if (!i)
	{
		ctl->success = FALSE;
		ctl->done = TRUE;
		return;
	}
	ctl->volume = i->volume;
	ctl->success = TRUE;
	ctl->done = TRUE;
}

static void pulse_ctl_success_cb(pa_context *c, int success, void *userdata)
{
	PulseCtl *ctl = (PulseCtl *) userdata;
	ctl->success = success ? TRUE : FALSE;
	ctl->done = TRUE;
}

static gboolean pulse_ctl_wait_context_ready(PulseCtl *ctl)
{
	pa_context_state_t state;

	while (TRUE)
	{
		state = pa_context_get_state(ctl->ctx);
		if (state == PA_CONTEXT_READY)
			return TRUE;
		if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED)
			return FALSE;
		if (pa_mainloop_iterate(ctl->ml, 1, NULL) < 0)
			return FALSE;
	}
}

static gboolean pulse_ctl_wait_done(PulseCtl *ctl)
{
	while (!ctl->done)
	{
		pa_context_state_t state = pa_context_get_state(ctl->ctx);
		if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED)
			return FALSE;
		if (pa_mainloop_iterate(ctl->ml, 1, NULL) < 0)
			return FALSE;
	}
	return ctl->success;
}

static gboolean pulse_ctl_init(PulseCtl *ctl)
{
	memset(ctl, 0, sizeof(*ctl));

	ctl->ml = pa_mainloop_new();
	if (!ctl->ml)
		return FALSE;

	ctl->ctx = pa_context_new(pa_mainloop_get_api(ctl->ml), "xmms");
	if (!ctl->ctx)
		return FALSE;

	if (pa_context_connect(ctl->ctx,
				       (pulse_cfg.server && *pulse_cfg.server) ? pulse_cfg.server : NULL,
				       PA_CONTEXT_NOFLAGS, NULL) < 0)
		return FALSE;

	return pulse_ctl_wait_context_ready(ctl);
}

static void pulse_ctl_cleanup(PulseCtl *ctl)
{
	if (ctl->ctx)
	{
		pa_context_disconnect(ctl->ctx);
		pa_context_unref(ctl->ctx);
	}
	if (ctl->ml)
		pa_mainloop_free(ctl->ml);
	g_free(ctl->default_sink);
}

static gboolean pulse_ctl_get_default_sink(PulseCtl *ctl)
{
	pa_operation *op;

	ctl->done = FALSE;
	ctl->success = FALSE;
	op = pa_context_get_server_info(ctl->ctx, pulse_ctl_server_info_cb, ctl);
	if (!op)
		return FALSE;
	pa_operation_unref(op);

	return pulse_ctl_wait_done(ctl) && ctl->default_sink && *ctl->default_sink;
}

static gboolean pulse_ctl_get_sink_volume(PulseCtl *ctl)
{
	pa_operation *op;

	ctl->done = FALSE;
	ctl->success = FALSE;
	op = pa_context_get_sink_info_by_name(ctl->ctx, ctl->default_sink, pulse_ctl_sink_info_cb, ctl);
	if (!op)
		return FALSE;
	pa_operation_unref(op);

	return pulse_ctl_wait_done(ctl);
}

static gboolean pulse_get_system_volume(gint *percent)
{
	PulseCtl ctl;
	gboolean ok = FALSE;
	gint vol;

	if (!percent)
		return FALSE;
	if (!pulse_ctl_init(&ctl))
		return FALSE;
	if (!pulse_ctl_get_default_sink(&ctl))
		goto out;
	if (!pulse_ctl_get_sink_volume(&ctl))
		goto out;

	vol = (gint) ((pa_cvolume_avg(&ctl.volume) * 100ULL) / PA_VOLUME_NORM);
	*percent = CLAMP(vol, 0, 100);
	ok = TRUE;

out:
	pulse_ctl_cleanup(&ctl);
	return ok;
}

static gboolean pulse_get_system_volume_cached(gint *percent)
{
	const gint64 now = g_get_monotonic_time();
	const gint64 cache_ttl_us = 1000 * 1000; /* 1s */

	if (percent == NULL)
		return FALSE;

	if (pulse_cached_system_volume >= 0 &&
	    now >= pulse_cached_system_volume_at &&
	    (now - pulse_cached_system_volume_at) < cache_ttl_us)
	{
		*percent = pulse_cached_system_volume;
		return TRUE;
	}

	if (!pulse_get_system_volume(percent))
		return FALSE;

	pulse_cached_system_volume = CLAMP(*percent, 0, 100);
	pulse_cached_system_volume_at = now;
	return TRUE;
}

static gboolean pulse_set_system_volume(gint percent)
{
	PulseCtl ctl;
	pa_cvolume vol;
	pa_operation *op;
	pa_volume_t pvol;
	gboolean ok = FALSE;
	guint i;

	if (!pulse_ctl_init(&ctl))
		return FALSE;
	if (!pulse_ctl_get_default_sink(&ctl))
		goto out;
	if (!pulse_ctl_get_sink_volume(&ctl))
		goto out;

	pvol = (pa_volume_t) ((((guint64) CLAMP(percent, 0, 100)) * PA_VOLUME_NORM) / 100ULL);
	vol = ctl.volume;
	for (i = 0; i < vol.channels; i++)
		vol.values[i] = pvol;

	ctl.done = FALSE;
	ctl.success = FALSE;
	op = pa_context_set_sink_volume_by_name(ctl.ctx, ctl.default_sink, &vol, pulse_ctl_success_cb, &ctl);
	if (!op)
		goto out;
	pa_operation_unref(op);

	ok = pulse_ctl_wait_done(&ctl);

out:
	pulse_ctl_cleanup(&ctl);
	return ok;
}

static void pulse_update_software_gains(gint left, gint right)
{
	gint base = MAX(left, right);

	pulse_cfg.volume_left = CLAMP(left, 0, 100);
	pulse_cfg.volume_right = CLAMP(right, 0, 100);
	pulse_hw_volume = CLAMP(base, 0, 100);

	if (pulse_hw_volume <= 0)
	{
		pulse_soft_gain_left = 100;
		pulse_soft_gain_right = 100;
		return;
	}

	pulse_soft_gain_left = CLAMP((pulse_cfg.volume_left * 100 + pulse_hw_volume / 2) / pulse_hw_volume, 0, 100);
	pulse_soft_gain_right = CLAMP((pulse_cfg.volume_right * 100 + pulse_hw_volume / 2) / pulse_hw_volume, 0, 100);
}

static void pulse_close_connection(void)
{
	if (!pulse_conn)
		return;
	pa_simple_free(pulse_conn);
	pulse_conn = NULL;
}

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

int pulse_open(AFormat fmt, int rate, int nch)
{
	pa_sample_spec ss;
	int err = 0;
	const char *server = NULL;
	const char *device = NULL;

	pulse_close_connection();

	if (!pulse_map_format(fmt, &pulse_format))
	{
		g_warning("pulse_open(): unsupported sample format %d", fmt);
		return 0;
	}

	ss.format = pulse_format;
	ss.rate = rate;
	ss.channels = nch;

	if (!pa_sample_spec_valid(&ss))
	{
		g_warning("pulse_open(): invalid sample spec");
		return 0;
	}

	if (pulse_cfg.server && *pulse_cfg.server)
		server = pulse_cfg.server;
	if (pulse_cfg.device && *pulse_cfg.device)
		device = pulse_cfg.device;

	pulse_conn = pa_simple_new(
		server,
		"xmms",
		PA_STREAM_PLAYBACK,
		device,
		(pulse_cfg.stream_name && *pulse_cfg.stream_name) ? pulse_cfg.stream_name : "XMMS",
		&ss,
		NULL,
		NULL,
		&err);

	if (!pulse_conn)
	{
		g_warning("pulse_open(): %s", pa_strerror(err));
		return 0;
	}

	pulse_channels = nch;
	pulse_bps = rate * nch * ((pulse_format == PA_SAMPLE_U8) ? 1 : 2);
	if (pulse_bps <= 0)
		pulse_bps = 1;
	pulse_written_bytes = 0;
	pulse_output_time_offset = 0;
	pulse_paused = FALSE;
	pulse_cached_system_volume = -1;
	pulse_cached_system_volume_at = 0;
	pulse_cached_latency_ms = 0;
	pulse_cached_latency_at = 0;

	return 1;
}

void pulse_write(void *ptr, int length)
{
	guchar *tmp = NULL;
	gboolean need_transform;
	int err = 0;

	if (!pulse_conn || !ptr || length <= 0 || pulse_paused)
		return;

	need_transform = pulse_convert_s8_to_u8 || pulse_convert_u16_to_s16 ||
			 pulse_soft_gain_left != 100 || pulse_soft_gain_right != 100;
	if (need_transform)
	{
		tmp = g_memdup(ptr, (gsize) length);
		pulse_transform_buffer(tmp, length);
		ptr = tmp;
	}

	if (pa_simple_write(pulse_conn, ptr, (size_t) length, &err) < 0)
		g_warning("pulse_write(): %s", pa_strerror(err));
	else
		pulse_written_bytes += (guint64) length;

	g_free(tmp);
}

void pulse_close(void)
{
	int err = 0;

	if (pulse_conn && !pulse_paused)
		pa_simple_drain(pulse_conn, &err);

	pulse_close_connection();
	pulse_written_bytes = 0;
	pulse_output_time_offset = 0;
	pulse_paused = FALSE;
	pulse_cached_system_volume = -1;
	pulse_cached_system_volume_at = 0;
	pulse_cached_latency_ms = 0;
	pulse_cached_latency_at = 0;
}

void pulse_flush(int time)
{
	int err = 0;

	if (pulse_conn)
		pa_simple_flush(pulse_conn, &err);
	pulse_written_bytes = 0;
	pulse_output_time_offset = (time >= 0) ? (guint64) time : 0;
}

void pulse_pause(short paused)
{
	int err = 0;
	pulse_paused = paused ? TRUE : FALSE;
	if (pulse_paused && pulse_conn)
		pa_simple_flush(pulse_conn, &err);
}

int pulse_free(void)
{
	if (!pulse_conn || pulse_paused)
		return 0;
	return 262144;
}

int pulse_playing(void)
{
	return (pulse_conn && !pulse_paused) ? TRUE : FALSE;
}

int pulse_get_written_time(void)
{
	if (!pulse_conn)
		return 0;
	return (gint) ((pulse_written_bytes * 1000ULL) / (guint64) pulse_bps);
}

int pulse_get_output_time(void)
{
	gint out;
	if (!pulse_conn)
		return 0;

	out = pulse_get_written_time();
	if (!pulse_paused)
	{
		const gint64 now = g_get_monotonic_time();
		const gint64 latency_ttl_us = 100 * 1000; /* 100ms */
		if (pulse_cached_latency_at == 0 ||
		    now < pulse_cached_latency_at ||
		    (now - pulse_cached_latency_at) >= latency_ttl_us)
		{
			int err = 0;
			pa_usec_t latency = pa_simple_get_latency(pulse_conn, &err);
			if (latency != (pa_usec_t) -1)
				pulse_cached_latency_ms = (gint) (latency / 1000);
			pulse_cached_latency_at = now;
		}
		out = MAX(0, out - pulse_cached_latency_ms);
	}
	return (gint) pulse_output_time_offset + out;
}

void pulse_get_volume(int *l, int *r)
{
	gint sys = -1;

	if (pulse_get_system_volume_cached(&sys))
	{
		if (pulse_hw_volume > 0)
		{
			if (l)
				*l = CLAMP((sys * pulse_soft_gain_left + 50) / 100, 0, 100);
			if (r)
				*r = CLAMP((sys * pulse_soft_gain_right + 50) / 100, 0, 100);
			return;
		}
		if (l)
			*l = sys;
		if (r)
			*r = sys;
		return;
	}

	if (l)
		*l = CLAMP(pulse_cfg.volume_left, 0, 100);
	if (r)
		*r = CLAMP(pulse_cfg.volume_right, 0, 100);
}

void pulse_set_volume(int l, int r)
{
	pulse_update_software_gains(l, r);

	if (!pulse_set_system_volume(pulse_hw_volume))
	{
		/* Fall back to pure software volume when Pulse sink volume cannot be set. */
		pulse_hw_volume = 100;
		pulse_soft_gain_left = CLAMP(pulse_cfg.volume_left, 0, 100);
		pulse_soft_gain_right = CLAMP(pulse_cfg.volume_right, 0, 100);
	}
	else
	{
		pulse_cached_system_volume = pulse_hw_volume;
		pulse_cached_system_volume_at = g_get_monotonic_time();
	}

	pulse_save_config();
}
