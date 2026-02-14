#include "pulse.h"
#include <string.h>

PulseConfig pulse_cfg;

void pulse_save_config(void)
{
	ConfigFile *cfgfile;

	cfgfile = xmms_cfg_open_default_file();
	if (!cfgfile)
		return;

	xmms_cfg_write_string(cfgfile, "PulseAudio", "server", pulse_cfg.server ? pulse_cfg.server : "");
	xmms_cfg_write_string(cfgfile, "PulseAudio", "device", pulse_cfg.device ? pulse_cfg.device : "");
	xmms_cfg_write_string(cfgfile, "PulseAudio", "stream_name", pulse_cfg.stream_name ? pulse_cfg.stream_name : "XMMS");
	xmms_cfg_write_int(cfgfile, "PulseAudio", "volume_left", CLAMP(pulse_cfg.volume_left, 0, 100));
	xmms_cfg_write_int(cfgfile, "PulseAudio", "volume_right", CLAMP(pulse_cfg.volume_right, 0, 100));
	xmms_cfg_write_default_file(cfgfile);
	xmms_cfg_free(cfgfile);
}

void pulse_init(void)
{
	ConfigFile *cfgfile;

	memset(&pulse_cfg, 0, sizeof(pulse_cfg));
	pulse_cfg.volume_left = 100;
	pulse_cfg.volume_right = 100;
	pulse_cfg.stream_name = g_strdup("XMMS");

	cfgfile = xmms_cfg_open_default_file();
	if (!cfgfile)
		return;

	xmms_cfg_read_string(cfgfile, "PulseAudio", "server", &pulse_cfg.server);
	xmms_cfg_read_string(cfgfile, "PulseAudio", "device", &pulse_cfg.device);
	if (!xmms_cfg_read_string(cfgfile, "PulseAudio", "stream_name", &pulse_cfg.stream_name))
	{
		g_free(pulse_cfg.stream_name);
		pulse_cfg.stream_name = g_strdup("XMMS");
	}
	xmms_cfg_read_int(cfgfile, "PulseAudio", "volume_left", &pulse_cfg.volume_left);
	xmms_cfg_read_int(cfgfile, "PulseAudio", "volume_right", &pulse_cfg.volume_right);
	pulse_cfg.volume_left = CLAMP(pulse_cfg.volume_left, 0, 100);
	pulse_cfg.volume_right = CLAMP(pulse_cfg.volume_right, 0, 100);

	xmms_cfg_free(cfgfile);
}
