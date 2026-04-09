#ifndef NAV_INVERT_H
#define NAV_INVERT_H

#include "misc.h"

/* UP/DOWN navigation inversion — runtime setting gSetting_nav_invert
 *   NAV_DIR(d)          — flips direction when invert is ON
 *   NAV_KEY_IS_UP(k)    — true if logical UP pressed
 *   NAV_KEY_IS_DOWN(k)  — true if logical DOWN pressed
 */

#define NAV_DIR(d)          (gSetting_nav_invert ? -(d) : (d))
#define NAV_KEY_IS_UP(k)    (gSetting_nav_invert ? ((k) == KEY_DOWN) : ((k) == KEY_UP))
#define NAV_KEY_IS_DOWN(k)  (gSetting_nav_invert ? ((k) == KEY_UP)   : ((k) == KEY_DOWN))

#endif
