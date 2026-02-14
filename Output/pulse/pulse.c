#include "pulse.h"

OutputPlugin pulse_op =
{
	NULL,
	NULL,
	NULL,
	pulse_init,
	pulse_about,
	pulse_configure,
	pulse_get_volume,
	pulse_set_volume,
	pulse_open,
	pulse_write,
	pulse_close,
	pulse_flush,
	pulse_pause,
	pulse_free,
	pulse_playing,
	pulse_get_output_time,
	pulse_get_written_time,
};

OutputPlugin *get_oplugin_info(void)
{
	pulse_op.description = g_strdup_printf(_("PulseAudio output plugin %s"), VERSION);
	return &pulse_op;
}
