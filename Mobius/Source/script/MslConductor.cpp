/**
 * The Manager for session lists within the MSL environment.
 *
 * Each MslContext thread has an active session list for the sessions
 * that are running within it, though in practice there are only
 * two, the shell and the kernel.  Unclear if we need a different
 * context for the maintenance thread.  Currently steps are taken
 * so that the UI thread and the maintenance thread block eaach other
 * so there is in effect only one shell level thread accessing the
 * environment at once.  There will only ever be a single thread in
 * kernel context.
 *
 * When control of a session passes from one context to another it is
 * called *transitioning*.  The session is removed from the source list
 * and placed in a Message that is sent to the other context.  The other
 * context receives the message during it's maintenance cycle and places
 * it on the active list.
 *
 * At regular intervals each context must call advance().  During the
 * advance phase these things happen.
 *
 *    - messagse from other contexts are processed
 *    - active sessions are advanced which may cause them to terminate
 *    - active sessions are *aged* which may cause them to terminate
 *
 * While a session is on the active list it may be in one of these states
 *
 *    running
 *       the session is being run at this time, normally a very temporary state
 *
 *    waiting
 *       the session entered a waite state and is waiting for an external
 *       notification to resume
 *
 *    suspended
 *       the session is not running or waiting
 *       it remains active and waits for an internal nofication or for
 *       it's suspended age to advance beyond a threshold
 *       it has no stack frames, but retains top-level variable binings
 *       currently used for #sustain and #repeat scripts
 *
 * An session terminates under these conditions:
 *
 *     - runs to completion and there are no suspension states or errors
 *     - ran to partial completion and has an error
 *     - has suspension states, but they have expired
 *     - is forcably canceled
 *
 * When a session terminates, it is removed from the active list and
 * discarded.  Final results, run statistics, and  error messages may
 * be deposited in a Result for monitoring.
 *
 * While a session is active it will have a Process which is accessible by
 * the monitorring UI.  There is a single Process list for all contexts.
 * A Process will be discarded as soon as the session terminates, the Result
 * will be kept indefinately.
 *
 * Process Monitoring
 *
 * The process list is accessible to all contexts and is unstable.  Any access
 * to the process list must be locked, including iteration.
 *
 * Results Monitoring
 *
 * The Results list is stable, it may be examined by the monitoring UI without locking.
 * They have indefinite lifespan until the user explicitly asks a result to be deleted
 * or the entire result list is pruned.a
 *
 * Results on this list are considered "interned".  Active sessions may CAREFULLY
 * add things to an interned result like final errors and values, or statistics but those
 * must be done as atomic operations on intrinsic values like numbers and pointers.
 * If a new result needs to be added to the list it is pushed on the front.  The monitor
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
#include "MslProcess.h"
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

    deleteMessageList(shellMessages);
    deleteMessageList(kernelMessages);
    
    deleteResultList(results);
    deleteProcessList(processes);
}

// todo: it would be nice if these could share a common
// interface for the chain pointer

void MslConductor::deleteSessionList(MslSession* list)
{
    while (list != nullptr) {
        MslSession* next = list->next;
        list->next = nullptr;
        delete list;
        list = next;
    }
}

void MslConductor::deleteProcessList(MslProcess* list)
{
    while (list != nullptr) {
        MslProcess* next = list->next;
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

void MslConductor::deleteMessageList(MslMessage* list)
{
    while (list != nullptr) {
        MslMessage* next = list->next;
        list->next = nullptr;
        delete list;
        list = next;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Note that suspended sessions are aged first.  This is because
 * if a session reaches a suspension state during this advance,
 * we don't want to treat the current advance as a suspension period.
 * It would get "one ahead" of where it should be.  Not a huge deal 
 * for the kernel since we're talking milliseconds but in the shell this
 * could be a 1/10 second or more which may be noticeable.
 *
 * This includes any sessions transitioning in, which may then suspended
 * when they are are advanced in the new context.
 *
 * Note also that since transitions happen with Messages, this means that
 * messsage handling happens after aging.  Normally this isn't an issue but
 * if we ever have "executive" messages like Cancel that should be processed
 * first, then we'll need to do message handling in two phases.
 */
void MslConductor::advance(MslContext* c)
{
    ageSuspended(c);
    
    consumeMessages(c);
    
    advanceActive(c);
}

//////////////////////////////////////////////////////////////////////
//
// Messages
//
//////////////////////////////////////////////////////////////////////

/**
 * Consume messages sent to this context.
 * Order is not significant.  The most common message is
 * for session transitioning from one context to another.
 */
void MslConductor::consumeMessages(MslContext* c)
{
    MslMessage* list = nullptr;
    
    if (c->mslGetContextId() == MslContextShell) {
        juce::ScopedLock lock (criticalSection);
        list = shellMessages;
        shellMessages = nullptr;
    }
    else {
        juce::ScopedLock lock (criticalSection);
        list = kernelMessages;
        kernelMessags = nullptr;
    }

    while (list != nullptr) {
        MslMessage* msg = list;
        MslMessage* next = list->next;
        msg->next = nullptr;

        switch (msg->type) {
            case MslMessage::MsgNone:
                Trace(1, "MslConductor: Received message with no type");
                break;
            case MslMessage::MsgTransition:
                doTransition(c, msg);
                break;
            case MslMessage::MsgNotification:
                doNotification(c, msg);
                break;
            case MslMessage::MsgCompletion:
                doCompletion(c, msg);
                break;
            case MslMessage::Result:
                doResult(c, msg);
                break;
        }

        // the doers are responsible for cleaning contents of the message
        messsagePool.checkin(msg);
    }
}

/**
 * Send a message from one context to another.
 * Since there are only two effective contexts, we don't need to pass
 * the destination, just go to the other side from where you are now.
 */
void MslConductor::sendMessage(MslContext* c, MslMessage* msg)
{
    if (c->mslGetContextId() == MslContextShell) {
        juce::ScopedLock lock (criticalSection);
        msg->next = kernelMessages;
        kernelMessages = msg;
    }
    else {
        juce::ScopedLock lock (criticalSection);
        msg->next = shellMessages;
        shellMessages = msg;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Transitions
//
//////////////////////////////////////////////////////////////////////

/**
 * Handle a transition message.
 * Don't need to lock the active lists here since the context
 * we're currently in owns them.
 */
void MslConductor::doTransition(MslContext* c, MslMessage* msg)
{
    MslSession* ses = msg->session;
    if (ses == nullptr) {
        Trace(1, "MslConductor: Transition message with no session");
    }
    else {
        addSession(ses);
        // the Process was givem StateTransitioning temporarily
        // to catch whether sessions get stuck transitioning
        // restore the actual state
        updateProcessState(ses);
    }
}

void MslConductor::addSession(MslContext* c, MslSession* s)
{
    if (c->mslGetContextId() == MslContextShell) {
        s->next = shellSessions;
        shellSessions = s;
    }
    else {
        s->next = kernelSessions;
        kernelSessions = s;
    }
}

void MslConductor::updateProcessState(MslSession* s)
{
    MslProcess* p = s->process;
    if (p != nullptr) {
        if (neu->isWaiting())
          p->state = MslProcess::StateWaiting;
        else if (neu->isSuspended())
          p->state = MslProcess::StateSuspended;
        else
          p->state = MslProcess::StateRunning;
    }
}

/**
 * Send a Session to the other side.
 */
void MslConductor::sendTransition(MslContext* c, MslSession* s)
{
    MslMessage* msg = messagePool.newMessage();
    msg->type = MslMessage::TypeTransition;
    msg->session = s;

    // temporary process state
    Process* p = s->process;
    if (p != null)
      p->state = MslProcess::StateTransitioning;

    sendMessage(c, msg);
}

/**
 * Called by Environment after a session is created and decides it
 * needs to transition.  At this point a Process is created so both
 * sides can monitor it.
 */
void MslConductor::addTransitioning(MslContext* c, MslSession* s)
{
    MslProcess* p = processPool.newProcess();
    p->sessionId = generateSessionId();
    p->state = MslProcess::StateTransitioning;
    
    // this pointer is unstable so we probably shouldn't even have it
    // Session can point back to it's Process but not the other way around
    p->session = s;
    s->setProcess(p);

    addProcess(p);
    sendTransition(c, s);
}

/**
 * Called by Environment aftar a session is created and enters a wait state.
 * If the session was created in the shell shouldn't be here since wait requires
 * a transition to the kernel first.  This commonly happens for sessions
 * created in the kernel though.
 *
 * Like transitioning, if we enter a wait state after launch, a Process is created
 * for monitoring.
 */
void MslConductor::addWaiting(MslContext* c, MslSession* s)
{
    MslProcess* p = processPool.newProcess();
    p->sessionId = generateSessionId();
    p->state = MslProcess::StateWaiting;
    
    // this pointer is unstable so we probably shouldn't even have it
    // Session can point back to it's Process but not the other way around
    p->session = s;
    s->setProcess(p);

    addProcess(p);
    addSession(s);
}

void MslConductor::addProcess(MslContext* c, MslProcess* p)
{
    (void)c;
    juce::ScopedLock lock (criticalSection);
    p->next = processes;
    processes = p;
}

/**
 * Generate a unique non-zero session id for a newly launched session.
 */
int MslConductor::generateSessionId()
{
    return ++sessionIds;
}

//////////////////////////////////////////////////////////////////////
//
// Active Session Advance
//
//////////////////////////////////////////////////////////////////////

/**
 * Process each of the active sessions.
 *
 * If a session completes and does not suspend is is removed from the
 * list and has results updated.
 */
void MslConductor::advanceActive(MslContext* c)
{
    MslSession* list = nullptr;
    if (c->mslGetContextId() == MslContextShell)
      list = shellSessions;
    else
      list = kernelSessions;
    
    MslSession* prev = nullptr;
    MslSession* current = list;

    while (current != nullptr) {
        MslSession* next = current->next;

        // environment does all the work
        environment->processSession(c, current);

        bool remove = false;
        if (current->hasErrors()) {
            // it doesn't matter what state it's in, as soon as an error
            // condition is reached, it terminates
            // todo: might want some tolerance here
            // you could have errors in one repeat, but move on to the next one
            // or errors in an OnSustain, but still want OnRelease to clean
            // something up
            // would be nice to have an optional OnError that always gets
            // called for cleanup
            terminate(c, current);
            remove = true;
        }
        else if (current->isTransitioning()) {
            // break on through to the other side
            sendTransition(c, current);
            remove = true;
        }
        else if (current->isWaiting()) {
            // it stays
            updateProcessState(currrent);
        }
        else if (current->isFinished()) {
            // it ran to completion without errors
            if (current->isSuspended()) {
                // but it gets to stay
                updateProcessState(current);
                // todo: any interesting statistics to leave
                // in the Process or Result?
            }
            else {
                // it's finally over, update Results
                updateResults(c, current);
                remove = true;
            }
        }
        else {
            // this is odd, it still has stack frames
            // but it isn't transitioning or waiting
            // shouldn't be here
            Trace(1, "MslConductor: Terminating session with mysterioius state");
            // todo: shold leave an error in the session that can be
            // transferred to the Result
            terminate(c, current);
            remove = true;
        }

        if (remove) {
            // splice it out of the list
            if (prev != nullptr)
              prev->next = next;
            else
              list = next;
        }
        else {
            prev = current;
        }
        
        current = next;
    }

    // put the modified list back
    if (c->mslGetContextId() == MslContextShell)
      shellSessions = list;
    else
      kernelSessions = list;
}

/**
 * Terminte a session due to an error condition.
 *
 * This case almost always causes errors to be transferred
 * to the Result.
 */
void MslConductor::terminate(MslContext* c, MslSession* s)
{
}

/**
 * Called by Environment after a session was started for the first time,
 * and after advanceActive after allowing it to resume after suspending.
 *
 * Check for varous ending states and take the appropriate action.
 */
void MslConductor::checkCompletion(MslContext* c, MslSession* s)
{
    if (s->hasErrors()) {
        // it doesn't matter what state it's in, as soon as an error
        // condition is reached, it terminates
        // todo: might want some tolerance here
        // you could have errors in one repeat, but move on to the next one
        // or errors in an OnSustain, but still want OnRelease to clean
        // something up
        // would be nice to have an optional OnError that always gets
        // called for cleanup
        terminate(c, s);
    }
    else if (s->isTransitioning()) {
        // break on through to the other side
        if (s->process == nullptr) {
            // must be the initial launch
            addTransitioning(c, s);
        }
        else {
            (void)removeSession(c, s);
            sendTransition(c, s);
        }
    }
    else if (current->isWaiting()) {
        // it stays here
        if (s->process == nullptr)
          addWaiting(c, s);
        updateProcessState(s);
    }
    else if (s->isFinished()) {
        // it ran to completion without errors
        if (s->isSuspended()) {
            if (s->process == nullptr)
              addWaiting(c, s);
            // but it gets to stay
            updateProcessState(current);
            // todo: any interesting statistics to leave
            // in the Process or Result?
        }
        else {
            finalize(c, s);
        }
    }
    else {
        // this is odd, it still has stack frames
        // but it isn't transitioning or waiting
        // shouldn't be here
        Trace(1, "MslConductor: Terminating session with mysterioius state");
        // todo: shold leave an error in the session that can be
        // transferred to the Result
        terminate(c, current);
        remove = true;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Finalization and Results
//
//////////////////////////////////////////////////////////////////////

/**
 * After a session has run to completion or been terminated,
 * clean up after it.
 * 
 * Save the final state in a Result object for the monitoring UI.
 * Results should only created when the session is over, while it is running
 * state ust be monitored throught the Process list.
 *
 * To avoid clutter, only create results if there were errors.
 * In the future, there need to be diagnostic options to always create results,
 * or allow the script to ask that results be created if they have something
 * to say.
 */
void MslConductor::finalize(MslContext* c, MslSession* s)
{
    MslProcess* p = s->getProcess();
    if (p != nullptr) {
        // this should be on the active list
        bool removed = removeSession(c, s);
        if (!removed)
          Trace(1, "MslConductor: Session with a Process was not on the list");
        removeProcess(p);
        // thanks for playing
        processPool->free(p);
    }
    else {
        // this should not be on the active list but make sure
        bool removed = removeSession(c, s);
        if (removed)
          Trace(1, "MslConductor: Session without a Process was on the list");
    }

    // todo: here we can have more complex decisions over whether
    // to save a result
    if (s->hasErrors()) {
        MslResult* r = makeResult(c, s);
        saveResult(c, r);
    }

    // you can go now, thank you for your service
    environment->getPool()->free(s);
}

bool MslConductor::removeProcess(MslContext* c, MslProcess* p)
{
    (void)c;
    bool removed = false;

    // this list is shared and must be locked
    {
        juce::ScopedLock lock (criticalSection);
        
        MslProcess* prev = nullptr;
        MslProcess* current = processes;
        while (current != nullptr) {
            MslProcess* next = current->next;
            if (current != p)
              prev = current;
            else {
                if (prev != nullptr)
                  prev->next = next;
                else
                  processes = next;
                current->next = nullptr;
                removed = true;
                break;
            }
        }
    }
    
    return removed;
}

/**
 * Remove the session from the active list.
 * Used for various reasons to get the session out of further consideration
 * by this context.
 */
bool MslConductor::removeSession(MslContext*c, MslSession* s)
{
    bool removed = false;
    
    MslSession* list = nullptr;
    if (c->mslGetContextId() == MslContextShell)
      list = shellSessions;
    else
      list = kernelSessions;

    MslSession* prev = nullptr;
    MslSession* current = list;

    while (current != nullptr) {
        MslSession* next = current->next;
        if (current != s)
          prev = current;
        else {
            if (prev != nullptr)
              prev->next = next;
            else
              list = next;
            current->next = nullptr;
            removed = true;
            break;
        }
    }

    if (removed) {
        // put the modified list back
        if (c->mslGetContextId() == MslContextShell)
          shellSessions = list;
        else
          kernelSessions = list;
    }
}

/**
 * After a session has run to completion or been terminated,
 * save the final state in a Result object for the monitoring UI.
 * Results should only created when the session is over, while it is running
 * state ust be monitored throught the Process list.
 *
 * To avoid clutter, only create results if there were errors.
 * In the future, there need to be diagnostic options to always create results,
 * or allow the script to ask that results be created if they have something
 * to say.
 *
 * It is the responsibility of the caller to decide if this is necessary.
 */
MslResult* MslConductor::makeResult(MslContext* c, MslSession* s)
{
    (void)c;
    // need to move the result pool in here
    MslResult* result = environment->getPool()->allocResult();

    // this is old, don't need this any more now that we have Process
    // but it does provide a unique identifier
    MslProcess* p = s->getProcess();
    if (p != nullptr) {
        result->sessionId = p->sessionId;
        // anything else from the process?
    }
    else {
        // this was a synchronous session with launch errors
        // for consistency generate a unique id
        result->sessionId = generateSessionId();
    }
    
    // give it a meaningful name if we can
    result->setName(s->getName());

    // transfer errors and result value
    result->errors = s->captureErrors();
    result->value = s->captureValue();

    return result;
}

/**
 * Save the final result on the result list if we're in the shell,
 * If not send it to the shell.
 * This avoids contention on the result list so the monitoring UI
 * doesn't have to mess with locking.
 */
void MslConductor::saveResult(MslContext* c, MslResult* r)
{
    if (c->mslGetContextId() == MslContextShell) {
        r->next = results;
        results = r;
    }
    else {
        MslMessage* msg = messagePool.newMessage();
        msg->type = MslMessage::TypeResult;
        msg->result = r;
        sendMessage(c, msg);
    }
}

/**
 * Handle a MsgResult message
 * These should only be sent by the kernel to the shell.
 */
void MslConductor::doResult(MslContext* c, MslMessage* msg)
{
    if (c->mslGetContextId() != MslContextShell) {
        Trace(1, "MslConductor: Result message sent to the wrong context");
    }
    else if (msg->result == nullptr) {
        Trace(1, "MslConductor: Result message missing result");
    }
    else {
        MslResult* r = msg->result;
        msg->result = nullptr;
        r->next = results;
        results = r;
    }
}

//////////////////////////////////////////////////////////////////////
// **** OLD ***
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
//
// Shell Maintenance
//
//////////////////////////////////////////////////////////////////////

/**
 * During the shell transition phase, queued sessions from the kernel are consumed.
 * Sessions that are still alive are placed on the shell active list and
 * sessions that have completed are removed and their results updated.
 *
 * The Result list is only maintained by the shell context.  When a session
 * termintates in the kernel, it is transitioned back to the shell for
 * results processing.
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
            // leave it off the active list, update result, and discard
            finishResult(neu);
        }
        else {
            // add it to the active list
            updateProcessState(neu);
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
 * Currently the two block in Mobius but this may not hold true in other
 * applications.
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

        // these clears StateTransitioning
        updateProcessState(current);
        
        bool remove = false;
        if (current->isTransitioning()) {
            juce::ScopedLock lock (criticalSection);
            current->next = toKernel;
            toKernel = current;
            MslProcess* p = current->getProcess();
            if (p != nullptr) p->state = MslProcess:StateTransitioning;
            remove = true;
        }
        else if (current->isSuspended() || current->isWaiting()) {
            // leave on the list, process state has been updated
        }
        
        
        

        
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

    sendMessage(c, msg);
}
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

/**
 * Retutn true if there is a process with this triggerId that is
 * suspendded and ready supports repeat.
 *
 * List has to be locked during the scan.
 */
MslProcess* MslConductor::findProcess(MslContext* c, int triggerId)
{
    MslProcess* found = nullptr;

    // the list is accessed from multiple threads to have to lock to scan
    {
        juce::ScopedLock lock (criticalSection);
        for (MslProcess* p = processes ; p != nullptr ; p = p->next) {
            if (p->triggerId == triggerId) {
                // 
                found = p;
                break;
            }
        }
    }

    return found;
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
