/**
 * Factory method to instantiate MobiusShell for use by the UI
 * Doesn't acomplish much except to hide MobiusShell and force the UI
 * to always go through MobiusInterface.
 */

#include "MobiusInterface.h"
#include "MobiusShell.h"

/**
 * Instantiate the instance of MobiusInterface.
 * This MUST be deleted by the caller.
 */
MobiusInterface* MobiusInterface::getMobius(MobiusContainer* container)
{
    return new MobiusShell(container);
}
