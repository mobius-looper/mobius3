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
#include "MslMessage.h"
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

    deleteMessageList(toShellMessages);
    deleteMessageList(toKernelMessages);

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

void MslConductor::deleteMessageList(MslMessage* list)
{
    while (list != nullptr) {
        MslMessage* next = list->next;
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

        if (neu->isFinished() && !neu->isSuspended()) {
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
 *
 * TODO: Potential issues if the maintenance thread that calls this
 * is allowed to be concurrent with the UI thread which is creating
 * new sessions or doing suspend/repeat notifications.
 * Currently the two block in Mobus but this shouldn't be assumed.
 *
 * This would have to remove the session from the list with a lock
 * in order to process it, then put it back with a lock if it didn't finish.
 */
void MslConductor::shellIterate(MslContext* context)
{
    MslSession* prev = nullptr;
    MslSession* current = shellSessions;

    while (current != nullptr) {
        MslSession* next = current->next;

        environment->processSession(context, current);

        // not liking the name "Finished"
        bool finished = current->isFinished() && !current->isSuspended();

        if (finished || current->isTransitioning()) {
            
            // splice it out of the list
            if (prev != nullptr)
              prev->next = next;
            else
              shellSessions = next;
            current->next = nullptr;

            if (finished) {
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

        bool finished = current->isFinished() && !current->isSuspended();

        if (finished || current->isTransitioning()) {
            
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
 * each other?  If it can be assured that the maintenance thread can't
 * be running at the same time as the UI thread could avoid this.  HOWEVER
 * this means MIDI events need to be sent through the UI thread since
 * those come in on their own special thread.  I believe this is true.
 *
 * Note that the lock here isn't enough if this is really an issue because
 * shellIterate isn't locking.
 *
 */
void MslConductor::addWaiting(MslContext* weAreHere, MslSession* s)
{
    if (weAreHere->mslGetContextId() == MslContextShell) {
        // this actually can't happen because Session will force a
        // kernel transition when setting up a wait state
        Trace(1, "MslConductor: Adding a waiting session to the shell list");
        
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
 * Move a session from one side to the other after it has been added.
 */
void MslConductor::transition(MslContext* weAreHere, MslSession* s)
{
    if (s != nullptr) {
        if (weAreHere->mslGetContextId() == MslContextShell) {
            bool foundit = false;
            {
                // scope lock on the "result" csect just in case we have
                // multiple shell threads touching sessions
                juce::ScopedLock lock (criticalSectionResult);
                foundit = remove(&shellSessions, s);
            }
            if (!foundit) {
                // it must have been added by now
                // don't just toss it on the kernel list without figuring out why
                Trace(1, "MslConductor: Transitioning session not on shell list");
            }
            else {
                juce::ScopedLock lock (criticalSection);
                s->next = toKernel;
                toKernel = s;
            }
        }
        else {
            // don't need to csect on the kernel list since there is only one thread
            bool foundit = remove(&kernelSessions, s);
            if (!foundit) {
                Trace(1, "MslConductor: Transitioning session not on kernel list");
            }
            else {
                juce::ScopedLock lock (criticalSection);
                s->next = toShell;
                toShell = s;
            }
        }
    }
}

/**
 * Splice a session out of a list.
 * Because we need to return both a status flag and a modification
 * to the head of the list, pass a pointer to the list head.
 * This must already be in an appropriate csect.
 */
bool MslConductor::remove(MslSession** list, MslSession* s)
{
    bool foundit = false;

    MslSession* prev = nullptr;
    MslSession* ptr = *list;
    while (ptr != nullptr && ptr != s) {
        prev = ptr;
        ptr = ptr->next;
    }
    
    if (ptr == s) {
        foundit = true;
        if (prev == nullptr)
          *list = ptr->next;
        else
          prev->next = ptr->next;
        ptr->next = nullptr;
    }
    return foundit;
}

//////////////////////////////////////////////////////////////////////
//
// Suspended
//
//////////////////////////////////////////////////////////////////////

/**
 * Return a session that is on the list matching the given triggerId.
 * TODO: THis has the UI/maintenance thread issue in the Shell context.
 */
MslSession* MslConductor::removeSuspended(MslContext* c, int triggerId)
{
    MslSession* found = nullptr;

    if (c->mslGetContextId() == MslContextShell) {
        MslSession* ses = shellSessions;
        while (ses != nullptr) {
            if (ses->getTriggerId() == triggerId) {
                found = ses;
                remove(&shellSessions, ses);
                break;
            }
        }
    }
    else {
        MslSession* ses = kernelSessions;
        while (ses != nullptr) {
            if (ses->getTriggerId() == triggerId) {
                found = ses;
                remove(&kernelSessions, ses);
                break;
            }
        }
    }
    return found;
}

/**
 * For repeating scripts, return true if there is a session with this triggerId
 * still active.
 *
 * !! This is a terrible and unreliable kludge walking over the other side's session
 * list which is unstable.  Need a safer way to determine this, but this allows progress
 * on repeat testing.
 *
 * The probe is just there to determine whether we need to do more work since most of the
 * time scripts will not repeat even if they allow it.
 */
bool MslConductor::probeSuspended(MslContext* c, int triggerId)
{
    (void)c;
    bool found = false;

    MslSession* ses = shellSessions;
    while (ses != nullptr) {
        if (ses->getTriggerId() == triggerId) {
            found = ses;
            break;
        }
    }

    if (!found) {
        ses = kernelSessions;
        while (ses != nullptr) {
            if (ses->getTriggerId() == triggerId) {
                found = ses;
                break;
            }
        }
    }
    
    return found;
}

void MslConductor::sendMessage(MslContext* c, MslNotificationFunction type, MslRequest* req)
{
    MslMessage* msg = messagePool->newMessage();

    msg->type = MslMessage::TypeNotification;
    msg->notification = type;
    msg->bindings = req->bindings;
    msg->arguments = req->arguments;

    // ownership transfers
    req->bindings = nullptr;
    req->arguments = nullptr;
    
    if (c->mslGetContextId() == MslContextShell) {
        juce::ScopedLock lock (criticalSection);
        msg->next = toKernelMessages;
        toKernelMessages = msg;
    }
    else {
        juce::ScopedLock lock (criticalSection);
        msg->next = toShellMessages;
        toShellMessages = msg;
    }
}

void MslConductor::consumeMessages(MslContext* c)
{
    MslMessage* messages = nullptr;
    
    if (c->mslGetContextId() == MslContextShell) {
        juce::ScopedLock lock (criticalSection);
        messages = toShellMessages;
        toShellMessages = nullptr;
    }
    else {
        juce::ScopedLock lock (criticalSection);
        messages = toKernelMessages;
        toKernelMessages = nullptr;
    }

    while (messages != nullptr) {
        MslMessage* next = messages->next;
        messages->next = nullptr;
        processMessage(c, messages);
        messages = next;
    }
}

void MslConductor::processMessage(MslContext* c, MslMessage* m)
{
    switch (m->type) {
        case MslMessage::MsgeNone:
            Trace(1, "MslConductor::processMessage Message with no type");
            break;
        case MslMessage::MsgTransition:
            Trace(1, "MslConductor::processMessage MsgTransition unexpected");
            break;
        case MslMessage::MsgNotification:
            doNotification(c, m);
            break;
        case MslMessage::MsgCompletion:
            Trace(1, "MslConductor::processMessage MsgCompletion unexpected");
            break;
        case MslMessage::MsgResult:
            Trace(1, "MslConductor::processMessage MsgResult unexpected");
            break;
    }

    // todo: return the bindings and arguments to the pool
    // rather than just deleting them
    m->clear();

    messagePool->checkin(m);
}

void MslConductor::doNotification(MslContext* c, MslMessage* m)
{
    switch (m->notification) {
        case MslMessage::MslNotificationRelease:
            doRelease(c, m);
            break;
        case MslMessage::MslNotificationRepeat:
            doRepeat(c, m);
            break;
        default:
            Trace(1, "MslConductor::doNotification Unexpected notification type %d",
                  m->notification);
            break;
    }
}

void MslConductor::doRelease(c, m)
{
    (void)c;
    (void)m;
    Trace(1, "MslConductor::doNotification Release not implemented");
}

void MslConductor::doRepeat(c, m)
{
    (void)c;
    (void)m;
    Trace(1, "MslConductor::doNotification Repeat not implemented");
}

#if 0
        case MslMessage::MsgTransition:
        
            
    MslSession* sessions = nullptr;
    
    if (c->mslGetContextId() == MslContextShell)
      sessions = shellSessions;
    else
      sessions = kernelSessions;

    MslSession* found = nullptr;
    for (MslSession* s = sessions ; s != nullptr ; s = s->next) {
        if (s->getTriggerId() == m->triggerId) {
            found = s;
            break;
        }
    }

    if (!found) {
        // must have been sent over at exactly the same time as the
        // message was sent
        // todo: return the message to the other side, but need a
        // governor to prevent infinite bouncing if the session went away
        Trace(1, "MslConductor::processMessage Target session not found");
        environment->getPool()->free(m);
    }
    else {
        environment->processMessage(c, m, found);
    }
}
#endif

void MslConductor::advanceSuspended(MslContext* c)
{
    MslSession* list = nullptr;
    
    if (c->mslGetContextId() == MslContextShell)
      list = shellSessions;
    else
      list = kernelSessions;

    MslSession* prev = nullptr;
    MslSession* session = list;
    while (session != nullptr) {
        MslSession* next = session->next;
        bool remove = false;
        
        MslSuspendState* state = session->getSustainState();
        if (state->isActive())
          remove = environment->processSustain(c, session);

        // if sustain ends it, then we don't need to check repeat timeouts
        if (!remove) {
            state = session->getRepeatState();
            if (state->isActive())
              remove = environment->processRepeat(c, session);
        }

        if (remove) {
            if (prev == nullptr)
              list = next;
            else
              prev->next = next;
            // environment will have already freed it
        }
        else {
            prev = session;
        }

        session = next;
    }

    if (c->mslGetContextId() == MslContextShell)
      shellSessions = list;
    else
      kernelSessions = list;
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
