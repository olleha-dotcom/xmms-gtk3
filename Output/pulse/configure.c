#include "pulse.h"

void pulse_configure(void)
{
	xmms_show_message(
		_("PulseAudio Output Plugin"),
		_("No GUI settings yet. Configure section 'PulseAudio' in ~/.xmms/config.\n"
		  "Keys: server, device, stream_name, volume_left, volume_right."),
		_("OK"),
		FALSE,
		NULL,
		NULL);
}
