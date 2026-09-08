#include <glib.h>
#include "xmms/playback.h"

static void healthy_playback(void)
{
    XmmsPlaybackMonitor monitor = {0};
    gint time;
    for (time = 230000; time <= 249000; time += 10)
        g_assert_false(xmms_playback_stalled_at_end(&monitor, 1, time, 247000,
            FALSE, (gint64) time * 1000));
}

static void final_second_stall(void)
{
    XmmsPlaybackMonitor monitor = {0};
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 1, 246500, 247000, FALSE, 1000000));
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 1, 246500, 247000, FALSE, 3999999));
    g_assert_true(xmms_playback_stalled_at_end(&monitor, 1, 246500, 247000, FALSE, 4000000));
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 1, 246500, 247000, FALSE, 5000000));
}

static void pause_seek_and_new_track(void)
{
    XmmsPlaybackMonitor monitor = {0};
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 1, 246500, 247000, FALSE, 1000000));
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 1, 246500, 247000, TRUE, 8000000));
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 1, 246500, 247000, FALSE, 9000000));
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 2, 246500, 247000, FALSE, 10000000));
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 2, 246500, 247000, FALSE, 12000000));
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 2, 100000, 247000, FALSE, 13000000));
    g_assert_false(xmms_playback_stalled_at_end(&monitor, 3, 246500, 247000, FALSE, 17000000));
}

static void no_metadata_or_early_stall(void)
{
    XmmsPlaybackMonitor monitor = {0};
    gint i;
    for (i = 0; i < 30; i++)
        g_assert_false(xmms_playback_stalled_at_end(&monitor, 1, 220000, 247000,
            FALSE, (gint64) i * G_USEC_PER_SEC));
    for (i = 0; i < 30; i++)
        g_assert_false(xmms_playback_stalled_at_end(&monitor, 2, 246500, -1,
            FALSE, (gint64) i * G_USEC_PER_SEC));
}

static void resume_policy(void)
{
    g_assert_true(xmms_resume_should_start(TRUE, TRUE, 42, FALSE, 2));
    g_assert_false(xmms_resume_should_start(FALSE, TRUE, 42, FALSE, 2));
    g_assert_false(xmms_resume_should_start(TRUE, FALSE, 42, FALSE, 2));
    g_assert_false(xmms_resume_should_start(TRUE, TRUE, 42, TRUE, 2));
    g_assert_false(xmms_resume_should_start(TRUE, TRUE, 42, FALSE, 0));
    g_assert_cmpint(xmms_resume_position(42, 120000), ==, 42);
    g_assert_cmpint(xmms_resume_position(999, 120000), ==, 119);
    g_assert_cmpint(xmms_resume_position(42, -1), ==, 42);
    g_assert_cmpint(xmms_resume_position(-4, 120000), ==, 0);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/playback/healthy-past-estimated-end", healthy_playback);
    g_test_add_func("/playback/final-second-stall", final_second_stall);
    g_test_add_func("/playback/pause-seek-new-track", pause_seek_and_new_track);
    g_test_add_func("/playback/no-metadata-early-stall", no_metadata_or_early_stall);
    g_test_add_func("/playback/resume-policy", resume_policy);
    return g_test_run();
}
