/**
 * Get a few enums out of the larger header files to reduce dependencies
 */

#pragma once

/**
 * Internal codes for the various notification functions we
 * may call automatiacally.
 */
typedef enum {

    MslNotificationNone,
    MslNotificationSustain,
    MslNotificationRepeat,
    MslNotificationRelease,
    MslNotificationTimeout

} MslNotificationFunction;

