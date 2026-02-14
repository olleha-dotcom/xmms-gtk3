#include "pulse.h"

void pulse_about(void)
{
	xmms_show_message(
		_("About PulseAudio Output Plugin"),
		_("XMMS PulseAudio output plugin for modern Ubuntu/PipeWire-Pulse systems."),
		_("OK"),
		FALSE,
		NULL,
		NULL);
}
