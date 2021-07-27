/*
 * System Interface Library for games
 * Copyright (c) 2007-2020 Andrew Church <achurch@achurch.org>
 * Released under the GNU GPL version 3 or later; NO WARRANTY is provided.
 * See the file COPYING.txt for details.
 *
 * src/sysdep/misc/joystick-db.c: Database of known HID joystick devices.
 */

#include "src/base.h"
#include "src/input.h"
#include "src/sysdep/misc/joystick-db.h"

/*************************************************************************/
/*************************************************************************/

/* The database itself is generated from the file "joystick-db.txt" in this
 * directory using the "gen-joystick-db-i.pl" script.  When building tests,
 * we add a few to exercise specific code paths in the tests. */
static const JoystickDesc joystick_db[] = {
#include "joystick-db.i"
#if defined(SIL_INCLUDE_TESTS) && defined(SIL_PLATFORM_LINUX)
    {.names = {"Linux test"},
     .ignore_vid_pid = 1,
     .lstick_x = JOYSTICK_VALUE_X,
     .lstick_y = JOYSTICK_VALUE_Y,
     .dpad_type = JOYSTICK_DPAD_BUTTONS,
     /* Note that this setup only has 3 buttons, so dpad_right is out of
      * range.  This is intentional. */
     .dpad_up = 0, .dpad_down = 1, .dpad_left = 2, .dpad_right = 3,
     .button_map = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}},
#endif
};

/*-----------------------------------------------------------------------*/

const JoystickDesc *joydb_lookup(int vendor_id, int product_id,
                                 uint32_t dev_version, const char *name)
{
    for (int i = 0; i < lenof(joystick_db); i++) {
        if (!joystick_db[i].ignore_name) {
            if (!name) {
                continue;
            } else {
                int match = 0;
                for (int j = 0; (j < lenof(joystick_db[i].names)
                                 && joystick_db[i].names[j]); j++) {
                    if (strcmp(name, joystick_db[i].names[j]) == 0) {
                        match = 1;
                        break;
                    }
                }
                if (!match) {
                    continue;
                }
            }
        }

        if (!joystick_db[i].ignore_vid_pid) {
            if (vendor_id != joystick_db[i].vendor_id
             || product_id != joystick_db[i].product_id) {
                continue;
            }
        }

        if ((dev_version & joystick_db[i].version_mask)
            != joystick_db[i].dev_version)
        {
            continue;
        }

        return &joystick_db[i];  // This entry matches the device.
    }

    return NULL;  // No match found.
}

/*************************************************************************/
/*************************************************************************/
