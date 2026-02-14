#ifndef XMMS_PULSE_H
#define XMMS_PULSE_H

#include "config.h"
#include "gtk_compat.h"

#include <pulse/simple.h>
#include <pulse/error.h>

#include <xmms/plugin.h>
#include <libxmms/configfile.h>
#include <libxmms/util.h>
#include <xmms/i18n.h>

typedef struct
{
	gchar *server;
	gchar *device;
	gchar *stream_name;
	gint volume_left;
	gint volume_right;
}
PulseConfig;

extern PulseConfig pulse_cfg;

void pulse_init(void);
void pulse_about(void);
void pulse_configure(void);
void pulse_save_config(void);

void pulse_get_volume(int *l, int *r);
void pulse_set_volume(int l, int r);

int pulse_open(AFormat fmt, int rate, int nch);
void pulse_write(void *ptr, int length);
void pulse_close(void);
void pulse_flush(int time);
void pulse_pause(short paused);
int pulse_free(void);
int pulse_playing(void);
int pulse_get_output_time(void);
int pulse_get_written_time(void);

#endif
