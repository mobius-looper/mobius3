/**
 * The Manager for session lists within the MLS environment.
 *
 * There are two phases: Transitioning and Iteration
 *
 * In the Transitioning phase sessions are moved from the transition
 * queue to the active list for whichever side is currently
 * touching the environment.
 *
 * In the Iteration phase, each session on the active list is Advanced.
 *
 * After Advancing the session can be in these states.
 *
 * finished
 *    The script ran out of things to do and the stack is empty
 *    The session may have a final result value
 *
 * canceled
 *    The script was asynchronously canceled before it reached completion
 *    The stack may still have frames for diagnostics about where it was
 *
 * aborted
 *    The script encountered a fatal error and stopped execution
 *    The stack may still have frames for diagnostics about where it was
 *    The error list will have information about what went wrong
 *
 * transitioning
 *    The script is still active, but needs to be transitioned to the
 *    other side to continue
 *
 * waiting
 *    The script is suspended on a wait state
 *
 * When a script is finished, canceled, or aborted, the session is moved
 * to the result list.
 *
 * When it is waiting, it is left on the active list.
 *
 * When it is transitioning it is moved from one active list to another.
 * 
 */

#include "MslSession.h"
#include "MslEnvironment.h"
#include "MslContext.h"
#include "MslConductor.h"

MslConductor::MslConductor(MslEnvironment* env)
{
    environment = env;
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
    deleteSessionList(results);
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
// Shell Maintenance
//
//////////////////////////////////////////////////////////////////////

/**
 * During the shell transition phase, all queued sessions from the kernel are consumed.
 * Sessions that are still alive are placed on the main shellSessions list and
 * sessions that have completed with results are placed on the results list.
 */
void MslConductor::shellTransition()
{
    // capture the incomming sessions
    // csect because kernel can be pushing at this very moment
    MslSession* neu = nullptr;
    {
        juce::ScopedLock lock (criticalSection);
        neu = toShell;
        toShell = nullptr;
    }

    while (neu != nullptr) {
        MslSession* next = neu->next;
        neu->next = nullptr;

        if (neu->isFinished()) {
            addResult(neu);
        }
        else {
            neu->next = shellSessions;
            shellSessions = neu;
        }
        neu = next;
    }
}

/**
 * Iterate over the active shell sessions.
 */
void MslConductor::shellIterate(MslContext* context)
{
    MslSession* prev = nullptr;
    MslSession* current = shellSessions;

    while (current != nullptr) {
        MslSession* next = current->next;

        environment->processSession(context, current);

        if (current->isFinished() || current->isTransitioning()) {
            
            // splice it out of the list
            if (prev != nullptr)
              prev->next = next;
            else
              shellSessions = next;
            current->next = nullptr;

            if (current->isFinished()) {
                addResult(current);
            }
            else {
                juce::ScopedLock lock (criticalSection);
                current->next = toKernel;
                toKernel = current;
            }
        }
        
        current = next;
    }
}

/**
 * Add a finished session from either the shell or the kernel to the
 * result list.
 *
 * todo: will need thread guards here once the SessionConsole
 * comes online and needs to iterate over it.
 */
void MslConductor::addResult(MslSession* s)
{
    juce::ScopedLock lock (criticalSectionResult);
    
    s->next = results;
    results = s;
}

/**
 * Called periodically by the maintenance thread to prune the result list.
 * Actually no, don't call this from the maintenance thread because it makes
 * it extremely difficult for the ScriptConsole and the MobiusConsole to show
 * the results of prior sessions without complex object locking.
 * Let these accumulate, at least if enabled in global config and prune
 * them manually when the user is in control.
 */
void MslConductor::pruneResults()
{
    MslSession* remainder = nullptr;
    {
        juce::ScopedLock lock (criticalSectionResult);
    
        // pick a number, any nummber
        int maxResults = 10;

        int count = 0;
        MslSession* s = results;
        while (s != nullptr && count < maxResults) {
            s = s->next;
            count++;
        }
        remainder = s->next;
        s->next = nullptr;
    }

    while (remainder != nullptr) {
        MslSession* next = remainder->next;
        remainder->next = nullptr;

        // don't have a session pool yet
        //environment->free(remainder);
        delete remainder;
        
        remainder = next;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Kernel Maintenance
//
//////////////////////////////////////////////////////////////////////

/**
 * The kernel transition phase is simpler because we don't have to deal
 * with adding finished sessions to the result list, they all go on the active list.
 */
void MslConductor::kernelTransition()
{
    // capture the incomming sessions
    // csect because shell can be pushing at this very moment
    MslSession* neu = nullptr;
    {
        juce::ScopedLock lock (criticalSection);
        neu = toKernel;
        toKernel = nullptr;
    }

    while (neu != nullptr) {
        MslSession* next = neu->next;
        neu->next = kernelSessions;
        kernelSessions = neu;
        neu = next;
    }
}

/**
 * Iterate over the active kernel sessions.
 * Both finished and transitioning sessions to back to the shell.
 */
void MslConductor::kernelIterate(MslContext* context)
{
    MslSession* prev = nullptr;
    MslSession* current = kernelSessions;

    while (current != nullptr) {
        MslSession* next = current->next;

        environment->processSession(context, current);

        if (current->isFinished() || current->isTransitioning()) {
            
            // splice it out of the list
            if (prev != nullptr)
              prev->next = next;
            else
              kernelSessions = next;
            current->next = nullptr;

            // and send it to the shell
            {
                juce::ScopedLock lock (criticalSection);
                current->next = toShell;
                toShell = current;
            }
        }
        
        current = next;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Launch Results
//
// The methods in this section are used after a new session is
// created in either context and it did not run to completion
// or it completed with errors.  In those cases it needs to
// be added to the appropriate list for later processing during
// the maintenance cycle.
//
//////////////////////////////////////////////////////////////////////

/**
 * Place the session in the opposite context from where we are now.
 */
void MslConductor::addTransitioning(MslContext* weAreHere, MslSession* s)
{
    if (weAreHere->mslGetContextId() == MslContextShell) {
        juce::ScopedLock lock (criticalSection);
        s->next = toKernel;
        toKernel = s;
    }
    else {
        juce::ScopedLock lock (criticalSection);
        s->next = toShell;
        toShell = s;
    }
}

/**
 * Place a non-transitioning but waiting session on the current
 * context's active list.  In practice, the current context
 * shold always be Kernel since waits can't be performed by the shell
 * at the moment, and this should have caused an immediate transition.
 * But someday it may make sense to have shell waits, like to respond
 * to a UI event.
 *
 * Needs more thought but infintely waiting shell sessions could be used
 * as "handlers" for things.
 *
 * Although we are on the "right side" the shell still needs to csect
 * protect the list because UIActions can be handled by both the UI
 * thread and the maintenance thread.  Really?  won't they be blocking
 * each other?
 */
void MslConductor::addWaiting(MslContext* weAreHere, MslSession* s)
{
    if (weAreHere->mslGetContextId() == MslContextShell) {
        // use the result csect since the kernel doesn't care
        juce::ScopedLock lock (criticalSectionResult);
        s->next = shellSessions;
        shellSessions = s;
    }
    else {
        // there are no threads in the kernel, so if that's where we are
        // then we have control
        s->next = kernelSessions;
        kernelSessions = s;
    }
}

/**
 * Place a session that immediately completed with errors on the result list.
 * If we're in the shell, it goes to the result list.
 * If we're in the kernel, it goes to toShell first, then the shell will
 * eventually move it to the result list.
 */
void MslConductor::addResult(MslContext* weAreHere, MslSession* s)
{
    if (weAreHere->mslGetContextId() == MslContextKernel) {
        juce::ScopedLock lock (criticalSection);
        s->next = toShell;
        toShell = s;
    }
    else {
        addResult(s);
    }
}

/**
 * This would be called by ScriptConsole to show what happened when recent script ran.
 * Important for scripts that ended up in the kernel because they may have hit errors
 * and those couldn't be conveyed to the user immediately.
 *
 * todo: currently race conditions here betwene the ScriptConsole displaying them
 * and the maintenance threads actively adding things to them.   Need a more
 * robust way to iterate over these or capture them.
 */
MslSession* MslConductor::getResults()
{
    return results;
}

/**
 * Hack to probe for session status after it was launched async.
 */
bool MslConductor::isWaiting(int id)
{
    bool waiting = false;
    MslSession* s = findSessionDangerously(id);
    if (s != nullptr) {
        waiting = s->isWaiting();
    }
    return waiting;
}

MslSession* MslConductor::getFinished(int id)
{
    return findSessionDangerously(results, id);
}

MslSession* MslConductor::findSessionDangerously(int id)
{
    MslSession* found = nullptr;

    // first look in results
    found = findSessionDangerously(results, id);

    if (found == nullptr)
      found = findSessionDangerously(shellSessions, id);

    if (found == nullptr)
      found = findSessionDangerously(kernelSessions, id);

    return found;
}

MslSession* MslConductor::findSessionDangerously(MslSession* list, int id)
{
    MslSession* found = nullptr;
    
    while (list != nullptr) {
        if (list->sessionId == id) {
            found = list;
            break;
        }
        list = list->next;
    }
    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
