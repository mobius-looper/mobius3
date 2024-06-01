/**
 * Factory method to instantiate MobiusShell for use by the UI
 *
 */

#include "MobiusInterface.h"
#include "MobiusShell.h"

/**
 * We don't really need to maintain a singleton but
 * it keeps the UI honest.
 */
MobiusInterface* MobiusInterface::Singleton = nullptr;

MobiusInterface* MobiusInterface::getMobius(MobiusContainer* container)
{
    if (Singleton == nullptr) {
        Singleton = new MobiusShell(container);
    }
    return Singleton;
}

void MobiusInterface::shutdown()
{
    delete Singleton;
    Singleton = nullptr;
}
