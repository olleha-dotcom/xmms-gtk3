#include <glib.h>
#include "Output/pulse/pulse.h"

PulseConfig pulse_cfg;
void pulse_save_config(void) {}

static void spin(guint milliseconds)
{
    gint64 end = g_get_monotonic_time() + milliseconds * 1000;
    do {
        while (g_main_context_iteration(NULL, FALSE));
        g_usleep(1000);
    } while (g_get_monotonic_time() < end);
}

static void open_stream(void)
{
    g_assert_true(pulse_open(FMT_S16_NE, 44100, 2));
    g_assert_cmpint(pulse_get_written_time(), ==, 0);
    g_assert_false(pulse_playing());
}

static void queue_audio(guint milliseconds)
{
    guint bytes = 44100 * 4 * milliseconds / 1000;
    guint8 *data = g_malloc0(bytes);
    pulse_write(data, bytes);
    g_free(data);
}

static void wait_drained(guint deadline_ms)
{
    gint64 end = g_get_monotonic_time() + deadline_ms * 1000;
    while (pulse_playing() && g_get_monotonic_time() < end)
        spin(10);
    g_assert_false(pulse_playing());
}

static void track_end_and_next(void)
{
    open_stream();
    queue_audio(300);
    g_assert_true(pulse_playing());
    wait_drained(4000);
    g_assert_cmpint(pulse_get_output_time(), ==, 300);
    g_assert_cmpint(pulse_get_written_time(), ==, 300);
    pulse_close();
    open_stream();
    queue_audio(100);
    wait_drained(4000);
    g_assert_cmpint(pulse_get_output_time(), ==, 100);
    pulse_close();
}

static void pause_keeps_audio(void)
{
    gint before, paused;
    gint64 resumed_at;
    open_stream();
    pulse_pause(TRUE);
    queue_audio(500);
    spin(150);
    paused = pulse_get_output_time();
    g_assert_cmpint(paused, ==, 0);
    g_assert_cmpint(pulse_get_written_time(), ==, 500);
    g_assert_true(pulse_playing());
    spin(200);
    g_assert_cmpint(pulse_get_output_time(), ==, paused);
    resumed_at = g_get_monotonic_time();
    pulse_pause(FALSE);
    wait_drained(4000);
    g_assert_cmpint(g_get_monotonic_time() - resumed_at, >=, 400000);
    g_assert_cmpint(pulse_get_output_time(), ==, 500);
    pulse_close();

    open_stream();
    queue_audio(1000);
    spin(250);
    before = pulse_get_output_time();
    pulse_pause(TRUE);
    spin(100);
    paused = pulse_get_output_time();
    g_assert_cmpint(paused, >=, before);
    g_assert_cmpint(paused - before, <=, 30);
    spin(100);
    g_assert_cmpint(pulse_get_output_time(), ==, paused);
    pulse_pause(FALSE);
    wait_drained(4000);
    g_assert_cmpint(pulse_get_written_time(), ==, 1000);
    pulse_close();
}

static void seek_and_stop(void)
{
    gint64 begin;
    open_stream();
    pulse_pause(TRUE);
    queue_audio(1000);
    pulse_flush(120000);
    g_assert_cmpint(pulse_get_written_time(), ==, 120000);
    g_assert_cmpint(pulse_get_output_time(), ==, 120000);
    queue_audio(200);
    g_assert_cmpint(pulse_get_written_time(), ==, 120200);
    pulse_pause(FALSE);
    wait_drained(4000);
    g_assert_cmpint(pulse_get_output_time(), ==, 120200);
    pulse_flush(5000);
    g_assert_cmpint(pulse_get_written_time(), ==, 5000);
    g_assert_cmpint(pulse_get_output_time(), ==, 5000);
    pulse_pause(TRUE);
    queue_audio(1000);
    begin = g_get_monotonic_time();
    pulse_close();
    g_assert_cmpint(g_get_monotonic_time() - begin, <, 200000);
    g_assert_false(pulse_playing());
}

static void volume_is_nonblocking(void)
{
    gint left, right, i;
    gint64 begin;
    spin(200);
    begin = g_get_monotonic_time();
    for (i = 0; i < 100; i++) {
        pulse_set_volume(50, 25);
        pulse_get_volume(&left, &right);
    }
    g_assert_cmpint(g_get_monotonic_time() - begin, <, 250000);
    spin(300);
    pulse_get_volume(&left, &right);
    g_assert_cmpint(left, >=, 49);
    g_assert_cmpint(left, <=, 51);
    g_assert_cmpint(right, >=, 24);
    g_assert_cmpint(right, <=, 26);
    pulse_set_volume(100, 100);
    spin(100);
}

static void low_rate_whole_blocks(void)
{
    guint8 data[2048] = {0}; /* WAV's 512 stereo frames at 1 kHz. */
    g_assert_true(pulse_open(FMT_S16_NE, 1000, 2));
    g_assert_cmpint(pulse_free(), >=, sizeof(data));
    pulse_write(data, sizeof(data));
    wait_drained(4000);
    g_assert_cmpint(pulse_get_output_time(), ==, 512);
    pulse_close();
}

static void pause_pending_drain(void)
{
    open_stream();
    queue_audio(1000);
    g_assert_true(pulse_playing());
    pulse_pause(TRUE);
    spin(300);
    g_assert_true(pulse_playing());
    g_assert_cmpint(pulse_get_output_time(), >=, 0);
    pulse_pause(FALSE);
    wait_drained(4000);
    g_assert_cmpint(pulse_get_output_time(), ==, 1000);
    pulse_close();
}

int main(int argc, char **argv)
{
    const char *server = g_getenv("XMMS_TEST_PULSE_SERVER");
    if (!server || !*server) {
        g_printerr("Use tests/run-pulse-test.sh to run against an isolated null sink.\n");
        return 77;
    }
    g_test_init(&argc, &argv, NULL);
    pulse_cfg.server = g_strdup(server);
    pulse_cfg.device = g_strdup("xmms_test");
    pulse_cfg.stream_name = g_strdup("XMMS regression test");
    pulse_cfg.volume_left = pulse_cfg.volume_right = 100;
    pulse_start();
    g_test_add_func("/pulse/end-and-next", track_end_and_next);
    g_test_add_func("/pulse/pause-keeps-audio", pause_keeps_audio);
    g_test_add_func("/pulse/seek-and-stop", seek_and_stop);
    g_test_add_func("/pulse/nonblocking-volume", volume_is_nonblocking);
    g_test_add_func("/pulse/low-rate-whole-blocks", low_rate_whole_blocks);
    g_test_add_func("/pulse/pause-pending-drain", pause_pending_drain);
    int result = g_test_run();
    pulse_shutdown();
    g_free(pulse_cfg.server);
    g_free(pulse_cfg.device);
    g_free(pulse_cfg.stream_name);
    return result;
}
