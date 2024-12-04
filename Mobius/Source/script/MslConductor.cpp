/**
 * The Manager for session lists within the MSL environment.
 *
 * update: also sucking in almost all of the old Environment code related
 * to session management
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

#include "MslConstants.h"
#include "MslContext.h"
#include "MslSession.h"
#include "MslResult.h"
#include "MslMessage.h"
#include "MslProcess.h"
#include "MslEnvironment.h"
#include "MslLinkage.h"

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

void MslConductor::enableResultDiagnostics(bool b)
{
    resultDiagnostics = b;
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
 * message handling happens after aging.  Normally this isn't an issue but
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
        kernelMessages = nullptr;
    }

    MslMessage* msg = list;
    while (msg != nullptr) {
        MslMessage* next = msg->next;
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
            case MslMessage::MsgResult:
                doResult(c, msg);
                break;
        }

        // the doers are responsible for cleaning contents of the message
        messagePool.checkin(msg);
        msg = next;
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
        addSession(c, ses);
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
        if (s->isWaiting())
          p->state = MslStateWaiting;
        else if (s->isSuspended())
          p->state = MslStateSuspended;
        else
          p->state = MslStateRunning;
    }
}

/**
 * Send a Session to the other side.
 */
void MslConductor::sendTransition(MslContext* c, MslSession* s)
{
    MslMessage* msg = messagePool.newMessage();
    msg->type = MslMessage::MsgTransition;
    msg->session = s;

    // temporary process state
    MslProcess* p = s->process;
    if (p != nullptr)
      p->state = MslStateTransitioning;

    sendMessage(c, msg);
}

/**
 * A session wants to transition. At this point a Process is created so both
 * sides can monitor it.
 */
void MslConductor::addTransitioning(MslContext* c, MslSession* s)
{
    MslProcess* p = makeProcess(s);
    p->state = MslStateTransitioning;
    addProcess(c, p);
    sendTransition(c, s);
}

MslProcess* MslConductor::makeProcess(MslSession* s)
{
    MslProcess* p = processPool.newProcess();
    p->sessionId = generateSessionId();

    // could avoid this if we just go through the session
    // since we have a pointer to it
    p->setName(s->getName());

    // this was saved here from the MslRequest, could have just passed
    // MslRequest everwhere within Conductor too since it started here
    p->triggerId = s->getTriggerId();

    // can't have one without the other?
    p->session = s;
    s->setProcess(p);

    return p;
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
    MslProcess* p = makeProcess(s);
    p->state = MslStateWaiting;
    addProcess(c, p);
    // if it waits it transitions first, and we're on the right side
    addSession(c, s);
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

/**
 * Capture the state of the process with this session id.
 */
bool MslConductor::captureProcess(int sessionId, MslProcess& result)
{
    bool found = false;
    juce::ScopedLock lock (criticalSection);
    for (MslProcess* p = processes ; p != nullptr ; p = p->next) {
        if (p->sessionId == sessionId) {
            result.copy(p);
            found = true;
            break;
        }
    }

    return found;
}

void MslConductor::listProcesses(juce::Array<MslProcess>& result)
{
    juce::ScopedLock lock (criticalSection);

    for (MslProcess* p = processes ; p != nullptr ; p = p->next) {
        // this feels messy
        MslProcess copy(p);
        result.add(copy);
    }
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
 * list and has results generated.
 *
 * Note that the checkCompletion() is shared by this caller and
 * by Environment when it launches a transient session for the first time.
 * checkCompletion() needs to do list management as well, so some of that
 * will be redundant since we're already iterating here, but it isn't worth
 * refactoring.  Just be careful that when you call checkCompletion the session
 * may be removed from the list we're iterating over here.  So be sure to capture
 * the next pointer before calling.
 */
void MslConductor::advanceActive(MslContext* c)
{
    MslSession* session = nullptr;
    if (c->mslGetContextId() == MslContextShell)
      session = shellSessions;
    else
      session = kernelSessions;
    
    while (session != nullptr) {
        // capture early before the list is modified
        MslSession* next = session->next;

        // resuming will cancel the transitioning state but not the waits
        session->resume(c);

        // decide what to do now, this may remove the session
        // from the list
        MslResult* result = checkCompletion(c, session);
        saveResult(c, result);
        
        session = next;
    }
}

/**
 * Called by Environment after a session was started for the first time,
 * and by advanceActive after allowing it to resume after suspending.
 *
 * Check for varous ending states and take the appropriate action.
 *
 * When called from Environment we won't have a Process yet so make one.
 * A Request is passed only when called from Environment, and ending state
 * can be stored there for synchronous return the application.
 */
MslResult* MslConductor::checkCompletion(MslContext* c, MslSession* s)
{
    MslResult* result = nullptr;
    
    if (s->hasErrors()) {
        // it doesn't matter what state it's in, as soon as an error
        // condition is reached, it terminates
        // todo: might want some tolerance here
        // you could have errors in one repeat, but move on to the next one
        // or errors in an OnSustain, but still want OnRelease to clean
        // something up
        // would be nice to have an optional OnError that always gets
        // called for cleanup
        result = finalize(c, s);
    }
    else if (s->isTransitioning()) {
        
        // break on through to the other side
        if (s->process == nullptr) {
            // must be the initial launch, not on a list yet
            addTransitioning(c, s);
            result = makeAsyncResult(s, MslStateTransitioning);
        }
        else {
            (void)removeSession(c, s);
            sendTransition(c, s);
        }
    }
    else if (s->isWaiting()) {
        // it stays here
        if (s->process == nullptr) {
            addWaiting(c, s);
            result = makeAsyncResult(s, MslStateWaiting);
        }
        updateProcessState(s);
    }
    else if (s->isFinished()) {
        // it ran to completion without errors
        if (s->isSuspended()) {
            // but it gets to stay
            if (s->process == nullptr) {
                addWaiting(c, s);
                result = makeAsyncResult(s, MslStateSuspended);
            }
            
            updateProcessState(s);
            // todo: any interesting statistics to leave
            // in the Process

            // todo: if the main body ran to completion it could
            // still return something through the Request even though
            // it is suspending
        }
        else {
            result = finalize(c, s);
        }
    }
    else {
        // this is odd, it still has stack frames but is not transitioning or waiting
        // can't happen without a logic error somewhere
        // force a termination to get it out of here
        Trace(1, "MslConductor: Terminating session with mysterioius state");
        // make sure the session has an error in it to take
        // the right path in finalize()
        s->addError("Abnormal termination");
        result = finalize(c, s);
    }

    return result;
}

MslResult* MslConductor::makeAsyncResult(MslSession* s, MslSessionState state)
{
    MslResult* r = environment->getPool()->allocResult();
    r->sessionId = s->getSessionId();
    r->state = state;
    return r;
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
MslResult* MslConductor::finalize(MslContext* c, MslSession* s)
{
    MslResult* result = makeResult(c, s);
    
    // this should be on the active list
    MslProcess* p = s->getProcess();
    if (p != nullptr) {
        bool removed = removeSession(c, s);
        if (!removed)
          Trace(1, "MslConductor: Session with a Process was not on the list");
        removeProcess(c, p);
        // thanks for playing
        processPool.checkin(p);
    }
    else {
        // this should not be on the active list but make sure
        bool removed = removeSession(c, s);
        if (removed)
          Trace(1, "MslConductor: Session without a Process was on the list");
    }

    // you can go now, thank you for your service
    environment->getPool()->free(s);

    return result;
}

/**
 * Splice a process out of the list.
 * Since the list is shared by both shell and kernel it needs to be locked.
 */
bool MslConductor::removeProcess(MslContext* c, MslProcess* p)
{
    (void)c;
    bool removed = false;
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
        current = next;
    }
    return removed;
}

/**
 * Remove the session from the active list.
 * Used for various reasons to get the session out of further
 * consideration by this context.
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
        current = next;
    }

    if (removed) {
        // if the list head was modified, have to put it back
        if (c->mslGetContextId() == MslContextShell)
          shellSessions = list;
        else
          kernelSessions = list;
    }

    return removed;
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
 * If the session ran to completion in the background (after transitioning
 * or waiting), results can't be returned synchronously to the application
 * launching the session.  If there were errors in the session, save
 * a persistent MslResult object that can be viewed later in the monitoring UI.
 *
 * Also allow save to be forced for diagnostics.
 */
void MslConductor::saveResult(MslContext* c, MslResult* result)
{
    if (result != nullptr) {

        if (!resultDiagnostics && result->errors == nullptr) {
            // nothing interesting to save
            environment->getPool()->free(result);
        }
        else if (c->mslGetContextId() == MslContextShell) {
            // can save it directly
            result->next = results;
            results = result;
        }
        else {
            // have to send it over
            MslMessage* msg = messagePool.newMessage();
            msg->type = MslMessage::MsgResult;
            msg->result = result;
            sendMessage(c, msg);
        }
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

/**
 * Called under user control to prune the result list.
 * Note that this can't be called periodically by the maintenance thread since
 * the ScriptConsole expects this list to be stable.
 *
 * This can only be called from the shell context.
 */
void MslConductor::pruneResults()
{
    MslResult* remainder = nullptr;
    
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
    
    while (remainder != nullptr) {
        MslResult* next = remainder->next;
        remainder->next = nullptr;

        // don't have a session pool yet
        //environment->free(remainder);
        delete remainder;
        
        remainder = next;
    }
}

/**
 * This would be called by ScriptConsole to show what happened when a recent script ran.
 * Important for scripts that ended up in the kernel because they may have hit errors
 * and those couldn't be conveyed to the user immediately.
 *
 * todo: currently a minor race conditions here betwene the ScriptConsole displaying them
 * the maintenance threads actively adding things to the list.  As long as it only
 * puts them on the head of the list and doesn't disrupt any of the chain pointers
 * it's safe, but still feels dirty.
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

//////////////////////////////////////////////////////////////////////
//
// Suspended Session Aging
//
//////////////////////////////////////////////////////////////////////

/**
 * Find any suspended sessions on the context list and advance their
 * wait states which may result in script notifications.
 *
 * If both #sustain and #repeat suspensions time out, then the session
 * goes through completion processing.
 */
void MslConductor::ageSuspended(MslContext* c)
{
    MslSession* list = nullptr;
    
    if (c->mslGetContextId() == MslContextShell)
      list = shellSessions;
    else
      list = kernelSessions;

    MslSession* session = list;
    while (session != nullptr) {
        // like advanceActive, this session can be reclaimed while
        // processing so get the next pointer now
        MslSession* next = session->next;
        bool mightBeDone = false;
        
        MslSuspendState* state = session->getSustainState();
        if (state->isActive()) {
            ageSustain(c, session, state);
            mightBeDone = true;
        }

        state = session->getRepeatState();
        if (state->isActive()) {
            ageRepeat(c, session, state);
            mightBeDone = true;
        }

        if (mightBeDone) {
            MslResult* r = checkCompletion(c, session);
            saveResult(c, r);
        }
        
        session = next;
    }
}

/**
 * Advance sustain state an call OnSuspend if we reach the threshold
 * Sustain does not currently have a timeout but we might want to add one.
 *
 * Subtle conflict: if you combine #sustain and #repeat they could have different
 * timeouts.  If they do, the higher of the two wins as far as finalizing
 * the script, but it will at least stop sending sustain notifications.
 */
void MslConductor::ageSustain(MslContext* c, MslSession* s, MslSuspendState* state)
{
    int now = juce::Time::getMillisecondCounter();
    int delta = now - state->timeoutStart;
    if (delta > state->timeout) {
        // bump the counter and re-arm for next time
        state->count++;
        state->timeoutStart = now;

        s->sustain(c);
    }
}

void MslConductor::ageRepeat(MslContext* c, MslSession* s, MslSuspendState* state)
{
    int now = juce::Time::getMillisecondCounter();
    int delta = now - state->timeoutStart;
    if (delta > state->timeout) {

        // this doesn't bump the counter or rearm, it means the repeat wait is over
        s->timeout(c);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Environment Requests
//
//////////////////////////////////////////////////////////////////////

/**
 * Start a new session for an initialization block.
 * todo: support an initial set of binding arguments?
 */
MslResult* MslConductor::runInitializer(MslContext* c, MslCompilation* unit,
                                        MslBinding* arguments, MslNode* node)
{
    MslResult* result = nullptr;
    if (node != nullptr) {
        MslSession* session = environment->getPool()->allocSession();

        // don't bump the run count for these
        
        session->run(c, unit, arguments, node);

        result = checkCompletion(c, session);
    }
    return result;
}


/**
 * Start a new session or repeat an existing one.
 */
MslResult* MslConductor::start(MslContext* c, MslRequest *req, MslLinkage* link)
{
    MslResult* result = nullptr;
    
    if (req->release) {
        // this can only do OnRelease on an existing session
        if (req->triggerId == 0) {
            Trace(1, "MslConductor: Release request with no trigger id");
        }
        else {
            // todo: once this locates the session it would be good to verify that
            // the link->function matches the session, but it isn't really necessary
            // because triggers can only do one thing
            // !! well until the point where you allow multiple bindings on one trigger
            // if you allow that then several layers from here on down will need to
            // be more complicated than just searching by triggerId, will need to pass
            // the Linkage through as well
            result = release(c, req);
        }
    }
    else {
        // see if we have a suspended repeat session for this trigger
        // we don't actually check to see if this is a repeat suspension
        // but there can only be one script bound to this triggere ATM
        MslContextId repeatContext = MslContextNone;

        // if they didn't pass a triggerId, could consider htat a failure
        // but it's also easy enough to fall back to simple start
        if (req->triggerId > 0)
          repeatContext = probeSuspended(c, req->triggerId);
        else {
            // this is common for scriptlets, don't whine
            //Trace(2, "MslConductor: Warning: No trigger id in request, couldn't check repeat");
        }

        if (repeatContext != MslContextNone) {
            result = repeat(c, req, repeatContext);
        }
        else {
            // a normal missionary position start
            MslSession* session = environment->getPool()->allocSession();

            // nice for the monitor
            link->runCount++;
        
            session->start(c, link, req);

            result = checkCompletion(c, session);
        }
    }

    return result;
}

/**
 * Here for a Request with a release action.
 * 
 * These make sense only for scripts that used #sustain which are normally still
 * waiting for the release.
 */
MslResult* MslConductor::release(MslContext* c, MslRequest* req)
{
    MslResult* result = nullptr;
    
    // is it on our side?
    MslSession* session = findSuspended(c, req->triggerId);
    if (session != nullptr) {
        // yes it is, let it go
        // we pass the Request in so OnRelease can have request arguments
        // rare but possible
        session->release(c, req);

        // the suspension is normall not active, should we force it?
        MslSuspendState* state = session->getSustainState();
        if (state->isActive()) {
            Trace(1, "MslConductor: Lingering sustain state after release");
            state->init();
        }
        result = checkCompletion(c, session);
    }
    else {
        // wasn't on this side, it's either on the other side or it
        // evaporated, due to an error on launch
        // before we send a message, make sure it's there

        MslContextId otherContext = probeSuspended(c, req->triggerId);
        if (otherContext == MslContextNone) {
            Trace(2, "MslConductor: Ignoring relase of terminated session");
        }
        else if (otherContext == c->mslGetContextId()) {
            // it's in our context, this can't happen if findSuspended is working
            // avoid sending a message so we don't loop
            Trace(1, "MslConductor: Stale context id in Process");
            // minor issue: there is a very rare race condition where if the other
            // side decided to transition the session to this context at the same time
            // as the probe, the Process context could be stale if the transition
            // is waiting in the message list.  Could peek into the message list
            // at this point to see if it's there.  Or could require a lock around
            // any MODIFICATION to a Process beyond just the process list which seems
            // kind of severe for such a rare case.  Still, keep an eye on it.
            // I suppse we could just send the message and let it bounce back too.
        }
        else {
            sendNotification(c, MslNotificationRelease, req);

            // todo: should have probeSuspended return the sessionId as well
            // so we can include that in an MslResult
        }
    }

    return result;
}

/**
 * Cause an OnRepeat notification after checking that the request
 * does in fact mean a repeat rather than just a simple start.
 */
MslResult* MslConductor::repeat(MslContext* c, MslRequest* req, MslContextId otherContext)
{
    MslResult* result = nullptr;
    
    // is it on our side?
    MslSession* session = findSuspended(c, req->triggerId);
    if (session != nullptr) {
        // yes it is, let it go
        // we pass the Request in so OnRelease can have request arguments
        // rare but possible
        session->repeat(c, req);

        // these normally don't complete until the timeout
        // unless there is a maxRepeat set
        result = checkCompletion(c, session);
    }
    else if (otherContext == c->mslGetContextId()) {
        // it's in our context, this can't happen if findSuspended is working
        // avoid sending a message so we don't loop
        // like release() this has a potential race condition with stale
        // context ids in the process
        Trace(1, "MslConductor: Stale context id in Process");
    }
    else {
        sendNotification(c, MslNotificationRepeat, req);
        // todo: should have probeSuspended return the sessionId as well
        // so we can include that in an MslResult
    }

    return result;
}

/**
 * Return a session that is on the local list matching the given triggerId.
 */
MslSession* MslConductor::findSuspended(MslContext* c, int triggerId)
{
    MslSession* found = nullptr;
    
    MslSession* list = nullptr;
    if (c->mslGetContextId() == MslContextShell)
      list = shellSessions;
    else
      list = kernelSessions;

    while (list != nullptr) {
        MslProcess* p = list->getProcess();
        if (p == nullptr) {
            Trace(1, "MslConductor: Active session with no process");
        }
        else if (p->triggerId == triggerId) {
            found = list;
            break;
        }
        list = list->next;
    }
    return found;
}

/**
 * Return true if there is a suspended session with a matching trigger id
 * in the OTHER context.
 * This must use the Process list since the opposing session list is
 * unstable.
 */
MslContextId MslConductor::probeSuspended(MslContext* c, int triggerId)
{
    (void)c;
    MslContextId otherContext = MslContextNone;
    {
        juce::ScopedLock lock (criticalSection);
        for (MslProcess* p = processes ; p != nullptr ; p = p->next) {
            if (p->triggerId == triggerId) {
                otherContext = p->context;
                break;
            }
        }
    }
    return otherContext;
}

void MslConductor::sendNotification(MslContext* c, MslNotificationFunction type, MslRequest* req)
{
    MslMessage* msg = messagePool.newMessage();

    msg->type = MslMessage::MsgNotification;
    msg->notification = type;
    msg->bindings = req->bindings;
    msg->arguments = req->arguments;

    // ownership transfers
    req->bindings = nullptr;
    req->arguments = nullptr;

    sendMessage(c, msg);
}

/**
 * Here when a Notification message comes in.
 * These can only be related to sustain and repeat right now.
 * Release and Timeout are handled during aging.
 */
void MslConductor::doNotification(MslContext* c, MslMessage* msg)
{
    switch (msg->notification) {
        case MslNotificationRelease:
            doRelease(c, msg);
            break;
        case MslNotificationRepeat:
            doRepeat(c, msg);
            break;
        default:
            Trace(1, "MslConductor: Unexpected notfication message %d", msg->notification);
            break;
    }
}

void MslConductor::doRelease(MslContext* c, MslMessage* msg)
{
    // make it look like we got the Request in this context
    MslRequest req;
    req.triggerId = msg->triggerId;
    req.bindings = msg->bindings;
    req.arguments = msg->arguments;

    // ownership transfers
    msg->bindings = nullptr;
    msg->arguments = nullptr;

    MslResult* res = release(c, &req);
    // this is a bagkround result so save it
    saveResult(c, res);
}

void MslConductor::doRepeat(MslContext* c, MslMessage* msg)
{
    // make it look like we got the Request in this context
    MslRequest req;
    req.triggerId = msg->triggerId;
    req.bindings = msg->bindings;
    req.arguments = msg->arguments;

    // ownership transfers
    msg->bindings = nullptr;
    msg->arguments = nullptr;

    MslResult* res = repeat(c, &req, c->mslGetContextId());
    saveResult(c, res);
}

/**
 * Here after a Wait statement has been scheduled in the MslContext
 * and the has come.  Normally in the kernel thread at this point.
 *
 * Setting the finished flag on the MslWait object will automatically pick
 * this up on the next maintenance cycle, but it is important that the
 * script be advanced synchronously now.
 *
 * Getting back to the MslSession what caused this is simple if it is stored
 * on the MslWait before sending it off.  We could also look in all the active
 * sessions for the one containing this MslWait object, but that's kind of a tedious walk
 * and it's easy enough just to save it.
 *
 * There is some potential thread contention here on the session if we allow waits
 * to happen in sessions at the shell level since there are more threads involved
 * up there than there are in the kernel.  That can't happen right now, but
 * if you do, then think about it here.
 */
void MslConductor::resume(MslContext* c, MslWait* wait)
{
    MslSession* session = wait->session;
    if (session == nullptr)
      Trace(1, "MslConductor: No session stored in MslWait");
    else {
        // this is the magic bean that makes it go
        wait->finished = true;

        session->resume(c);

        MslResult* result = checkCompletion(c, session);
        // we're in the background so save if errors
        saveResult(c, result);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
