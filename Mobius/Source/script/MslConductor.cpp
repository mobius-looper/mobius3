/**
 * The Manager for session lists within the MLS environment.
 *
 */

#include "MslSession.h"
#include "MslEnvironment.h"
#include "MslConductor.h"

MslConductor::MslConductor()
{
}

/**
 * Upon destruction, reclaim anything that remains on the lists.
 * Things do not need to be returned to pools since those pools
 * are being destroyed as well.
 */
MslConductor::~MslConductor()
{
    deleteSessionList(shellSessions);
    deleteSessionList(kernelSessions);
    deleteSessionList(toShell);
    deleteSessionList(toKernel);
}

void MslConductor::deleteSessionList(MslSession* list)
{
    while (list != nullptr) {
        MslSession* next = list->next;
        list->next = nullptr;
        delete list;
        list = next;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Shell
//
//////////////////////////////////////////////////////////////////////

/**
 * Return the list of sessions the shell needs to process.
 * This is the full session list and the chain pointers can be assumed
 * valid until shellMaintenance is called.  If a new session needs to be
 * added it must be pushed on the front of the list so as not to disrupt
 * iteration of the original list.  
 */
MslSession* MslConductor::getShellSessions()
{
    return shellSessions;
}

/**
 * Add a session to the shell list.
 * This can be called both by shellMaintenance in this class
 * as well as MslEnvironment::shellExecute as it iterates over the
 * list returned by getShellSessions.  So additions must be pushed to the
 * head of the sessions list to avoid disruption of the chain pointers.
 */
void MslConductor::addShellSession(MslSession* s)
{
    s->next = shellSessions;
    shellSessions = next;
}

/**
 * 



//////////////////////////////////////////////////////////////////////
//
// Kernel
//
//////////////////////////////////////////////////////////////////////

