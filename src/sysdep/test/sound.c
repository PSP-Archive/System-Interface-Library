/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/test/sound.c: Testing implementation of the system-level
 * audio output interface.
 */

#define IN_SYSDEP_TEST

#include "src/base.h"
#include "src/math.h"
#include "src/memory.h"
#include "src/sound.h"
#include "src/sound/mixer.h"
#include "src/sysdep.h"
#include "src/sysdep/test.h"

/*************************************************************************/
/********************* Test control data (exported) **********************/
/*************************************************************************/

/* To enable the testing of the system's real sound routines alongside
 * these test routines, we redo the sys_* stubs defined in sysdep.h to
 * switch on a sound-specific flag. */

uint8_t sys_test_sound_use_live_routines = 0;

#undef sys_sound_init
#undef sys_sound_playback_rate
#undef sys_sound_set_latency
#undef sys_sound_enable_headphone_disconnect_check
#undef sys_sound_check_headphone_disconnect
#undef sys_sound_acknowledge_headphone_disconnect
#undef sys_sound_cleanup

#define DEFINE_STUB(type,name,params,args) \
extern type name params; \
static type TEST__##name params; \
type TEST_##name params { \
    return sys_test_sound_use_live_routines \
        ? name args : TEST__##name args; \
}
DEFINE_STUB(int, sys_sound_init, (const char *device_name), (device_name))
DEFINE_STUB(int, sys_sound_playback_rate, (void), ())
DEFINE_STUB(float, sys_sound_set_latency, (float latency), (latency))
DEFINE_STUB(void, sys_sound_enable_headphone_disconnect_check, (int enable), (enable))
DEFINE_STUB(int, sys_sound_check_headphone_disconnect, (void), ())
DEFINE_STUB(void, sys_sound_acknowledge_headphone_disconnect, (void), ())
DEFINE_STUB(void, sys_sound_cleanup, (void), ())

#define sys_sound_init                  TEST__sys_sound_init
#define sys_sound_playback_rate         TEST__sys_sound_playback_rate
#define sys_sound_set_latency           TEST__sys_sound_set_latency
#define sys_sound_enable_headphone_disconnect_check TEST__sys_sound_enable_headphone_disconnect_check
#define sys_sound_check_headphone_disconnect TEST__sys_sound_check_headphone_disconnect
#define sys_sound_acknowledge_headphone_disconnect TEST__sys_sound_acknowledge_headphone_disconnect
#define sys_sound_cleanup               TEST__sys_sound_cleanup

/*************************************************************************/
/****************************** Local data *******************************/
/*************************************************************************/

/* Sampling rate to use for mixing. */
static int output_rate = 48000;

/* Have we been initialized? */
static uint8_t sound_initted = 0;

/* Current latency, in sample periods.  The default latency is one sample
 * period (1.0 / output_rate). */
static int sample_latency;

/* Is headphone disconnect checking enabled? */
static uint8_t headphone_disconnect_enabled;

/* Should we report a headphone disconnect? */
static uint8_t headphone_disconnect_flag;

/*************************************************************************/
/************************** Interface routines ***************************/
/*************************************************************************/

int sys_sound_init(const char *device_name)
{
    PRECOND(device_name != NULL, return 0);
    PRECOND(!sound_initted, return 0);

    /* Magic values used in sound core tests. */
    if (strcmp(device_name, "FAIL") == 0) {
        DLOG("Failing as requested");
        return 0;
    }
    if (strcmp(device_name, "NEGA") == 0) {
        DLOG("Setting output_rate to -1 as requested");
        output_rate = -1;  // Will cause sound_init() to fail.
    } else if (strcmp(device_name, "ZERO") == 0) {
        DLOG("Setting output_rate to 0 as requested");
        output_rate = 0;  // Will disable decoder resampling.
    }

    headphone_disconnect_enabled = 0;
    headphone_disconnect_flag = 0;

    sound_initted = 1;
    return 1;
}

/*-----------------------------------------------------------------------*/

int sys_sound_playback_rate(void)
{
    return output_rate;
}

/*-----------------------------------------------------------------------*/

float sys_sound_set_latency(float latency)
{
    PRECOND(output_rate > 0);
    if (latency > 0) {
        sample_latency = lbound(iroundf(latency * output_rate), 1);
    }
    return sample_latency / (float)output_rate;
}

/*-----------------------------------------------------------------------*/

void sys_sound_enable_headphone_disconnect_check(int enable)
{
    headphone_disconnect_enabled = (enable != 0);
}

/*-----------------------------------------------------------------------*/

int sys_sound_check_headphone_disconnect(void)
{
    return headphone_disconnect_flag;
}

/*-----------------------------------------------------------------------*/

void sys_sound_acknowledge_headphone_disconnect(void)
{
    headphone_disconnect_flag = 0;
}

/*-----------------------------------------------------------------------*/

void sys_sound_cleanup(void)
{
    PRECOND(sound_initted, return);

    sound_initted = 0;
}

/*************************************************************************/
/************************* Test control routines *************************/
/*************************************************************************/

void sys_test_sound_set_output_rate(int rate)
{
    PRECOND(!sound_initted, return);
    output_rate = rate;
}

/*-----------------------------------------------------------------------*/

void sys_test_sound_set_headphone_disconnect(void)
{
    PRECOND(headphone_disconnect_enabled, return);
    headphone_disconnect_flag = 1;
}

/*************************************************************************/
/*************************************************************************/
