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
 * RESULTS
 * 
 * Management of result lists is a little complicated due to threading issues.
 *
 * Once somethign lands on the main results list, it may be examined by the ScriptConsole
 * or other things in the UI not under the environment's control.  The MslResult chain
 * pointers MUST remain stable, the console will iterate over that without locking
 * and it is assumed they have indefinite lifespan until the user explicitly asks
 * to prune results.
 *
 * Results on this list are considered "interned".  Active sessions may CAREFULLY
 * add things to an interned result like final errors and values, or statistics but those
 * must be done as atomic operations on intrinsic values like numbers and pointers.
 * If a new result needs to be added to the list it is pushed on the front.  The console
 * doesn't care about new results as long as the chain of results it is now dealing with
 * remains stable.
 *
 * Both shell threads and the kernel threads need to push new results onto the list
 * and that must be guarded by a critical section.
 * 
 */

#include "../util/Trace.h"

#include "MslSession.h"
#include "MslResult.h"
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

    deleteResultList(results);
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

void MslConductor::deleteResultList(MslResult* list)
{
    while (list != nullptr) {
        MslResult* next = list->next;
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
 * sessions that have completed are removed and their results updated.
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
            finishResult(neu);
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
                finishResult(current);
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
 * Add a new result to the interned result list.
 * The result may still be associated with an active session, or
 * it may what was left behind from a session that immediately ran to completion
 * without transitioning.
 */
void MslConductor::addResult(MslResult* r)
{
    juce::ScopedLock lock (criticalSectionResult);
    r->next = results;
    results = r;
}

/**
 * Called when an background session has finished.
 * It has been removed from the shell or kernel session list.
 * Move any final state from the session to the result and
 * release the session back to the pool.
 *
 * An associated MslResult should always have been created by now.
 * todo: Who has the responsibility for refreshing the result, conductor
 * or do we allow the session to touch the result along the way?
 */
void MslConductor::finishResult(MslSession* s)
{
    MslPools* pool = environment->getPool();
    
    MslResult* result = s->result;
    if (result == nullptr) {
        // I suppose we could generate a new one here, but Environment
        // is supposed to have already done that
        Trace(1, "MslConductor: Finished session without a result");
    }
    else {
        // the console may be examining the object so only allowed to do
        // atomic things
        // !! this is a problem since we don't incrementally add things
        // like more errors on a list or a different value this is safe,
        // but I don't like how this is working.  Really don't want to have
        // console locking these, perhaps we need two lists, one for dead sessions
        // and one for those still running
        
        MslError* oldErrors = result->errors;
        if (oldErrors != nullptr)
          Trace(1, "MslConductor: Replacing result error list!");
        result->errors = s->captureErrors();
        pool->free(oldErrors);

        MslValue* oldValue = result->value;
        if (oldValue != nullptr)
          Trace(1, "MslConductor: Replacing result value!");
        result->value = s->captureValue();
        pool->free(oldValue);

        // todo: interesting execution statistics

        // take away this pointer, which really shouldn't have been there anyway
        result->session = nullptr;
    }

    // and finally reclaim the session
    pool->free(s);
}

/**
 * Called under user control to prune the result list.
 * Note that this can't be called periodically by the maintenance thread since
 * the ScriptConsole expects this list to be stable.
 */
void MslConductor::pruneResults()
{
    MslResult* remainder = nullptr;
    {
        juce::ScopedLock lock (criticalSectionResult);
    
        // pick a number, any nummber
        int maxResults = 10;

        int count = 0;
        MslResult* s = results;
        while (s != nullptr && count < maxResults) {
            s = s->next;
            count++;
        }
        remainder = s->next;
        s->next = nullptr;
    }

    while (remainder != nullptr) {
        MslResult* next = remainder->next;
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
            // actually we could do the same finishResult processing
            // that shell does here, but keep doing it the old way
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
// Transitions
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

//////////////////////////////////////////////////////////////////////
//
// Result Access
//
//////////////////////////////////////////////////////////////////////


/**
 * This would be called by ScriptConsole to show what happened when recent script ran.
 * Important for scripts that ended up in the kernel because they may have hit errors
 * and those couldn't be conveyed to the user immediately.
 *
 * todo: currently race conditions here betwene the ScriptConsole displaying them
 * and the maintenance threads actively adding things to them.   Need a more
 * robust way to iterate over these or capture them.
 */
MslResult* MslConductor::getResults()
{
    return results;
}

/**
 * Find a specific result by id.
 */
MslResult* MslConductor::getResult(int id)
{
    MslResult* found = nullptr;
    
    MslResult* ptr = results;
    while (ptr != nullptr) {
        if (ptr->sessionId == id) {
            found = ptr;
            break;
        }
        ptr = ptr->next;
    }
    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
