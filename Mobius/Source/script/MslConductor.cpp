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
#include "MslFunction.h"
#include "MslCompilation.h"

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
            case MslMessage::MsgRequest:
                doRequest(c, msg);
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
        MslProcess* p = s->process;
        if (p != nullptr)
          p->context = MslContextShell;
        else
          Trace(1, "MslConductor: Expecting to have an MslProcess by now");
    }
    else {
        s->next = kernelSessions;
        kernelSessions = s;
        MslProcess* p = s->process;
        if (p != nullptr)
          p->context = MslContextKernel;
        else
          Trace(1, "MslConductor: Expecting to have an MslProcess by now");
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

    // keep track or error stats for the monitor
    // todo: session doesn't always have a Linkage, might be better on the unit?
    if (result->errors != nullptr) {
        MslLinkage* link = s->getLinkage();
        if (link != nullptr)
          link->errorCount++;
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
    result->results = s->captureResults();
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
 * If the script adds detailed results with AddResult, then keep it as well.
 */
void MslConductor::saveResult(MslContext* c, MslResult* result)
{
    if (result != nullptr) {

        if (!resultDiagnostics && result->errors == nullptr &&
            result->results == nullptr) {
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
    juce::uint32 now = juce::Time::getMillisecondCounter();
    juce::uint32 delta = now - state->timeoutStart;
    if (delta > state->timeout) {
        // bump the counter and re-arm for next time
        state->count++;
        state->timeoutStart = now;

        s->sustain(c);
    }
}

void MslConductor::ageRepeat(MslContext* c, MslSession* s, MslSuspendState* state)
{
    juce::uint32 now = juce::Time::getMillisecondCounter();
    juce::uint32 delta = now - state->timeoutStart;
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

MslResult* MslConductor::run(MslContext* c, MslCompilation* unit,
                             MslBinding* arguments)
{
    MslResult* result = nullptr;

    MslFunction* func = unit->getBodyFunction();
    if (func != nullptr) {
        MslBlockNode* body = func->getBody();
        if (body != nullptr) {
            MslSession* session = environment->getPool()->allocSession();
            session->run(c, unit, arguments, body);
            result = checkCompletion(c, session);
        }
    }
    return result;
}

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
 * Handle a user request.
 * This will either start a new session, or resume a suspended session.
 *
 * If a triggerId was passed in the request, see if there is already
 * a session for that trigger.  If a t riggerId is not passed, then
 * this can only launch new sessions, it can't resume #sustain or #repeat sessions
 */
MslResult* MslConductor::request(MslContext* c, MslRequest* req)
{
    MslResult* result = nullptr;

    // todo: if this is a release request should we check now to see
    // if this script is even sustainable?  could prevent some useless
    // message passing
    
    MslSession* session = nullptr;
    if (req->triggerId > 0) {
        session = findSuspended(c, req->triggerId);
        if (session != nullptr) {
            // it is on our side
            result = resume(c, req, session);
        }
        else {
            // just because we don't have it here, doesn't mean it isn't over there
            MslContextId other = probeSuspended(c, req->triggerId);
            if (other == MslContextNone) {
                if (req->release) {
                    // the session must may have errored while waiting for release
                    // todo: this could be common, who checks to see if a release request
                    // should even be sent?  Binderator?
                    Trace(1, "MslConductor: Ignoring release request for unknown session");
                }
                else {
                    // no sessions exist, start a new one
                    result = start(c, req);
                }
            }
            else if (other == c->mslGetContextId()) {
                // it thinks it is here but we didn't find it, shouldn't
                // happen, this probably means there is an orphaned Process
                Trace(1, "MslConductor: Inconsistent suspended session context");
            }
            else {
                // there is a session for this trigger on the other side, send it over
                sendRequest(c, req);
            }
        }
    }
    else if (req->release) {
        // it's a release event but they didn't pass a triggerId which
        // is an error, shouldn't have bothered with the request at all
        Trace(1, "MslConductor: Release request without trigger id");
    }
    else {
        // no trigger id, can only start
        // here we might want to check whether concurrency is allowed
        result = start(c, req);
    }
    return result;
}

/**
 * Message handler for the MslRequest message
 */
void MslConductor::doRequest(MslContext* c, MslMessage* msg)
{
    if (msg->request.triggerId == 0) {
        // shouldn't have bothered with a Message if there wasn't a known trigger
        Trace(1, "MslConductor: Invalid request trigger id");
    }
    else {
        MslSession* session = findSuspended(c, msg->request.triggerId);
        if (session != nullptr) {
            // what we expected
            MslResult* result = resume(c, &(msg->request), session);
            saveResult(c, result);
        }
        else {
            // we thought there was a suspended session on this side,
            // but now that we're here it isn't there (if that make sense)
            // this could be due to an orphaned Process which is unexpected,
            // could also be due to the session transitioning at exactly the same time
            // as the Request which is possible but extremely rare
            // it also happens during debugging if a suspension times out while stopped
            // on a breakpoint so the session is gone by the time we get here
            // todo: could redo the Request now and start a new session
            Trace(1, "MslConductor: Expected suspended session evaporated");
        }
    }

    // reclaim anything left behind in the Request since this was a copy
    environment->free(msg->request.bindings);
    environment->free(msg->request.arguments);
    msg->request.bindings = nullptr;
    msg->request.arguments = nullptr;
}

/**
 * Here when we've got a request for a session with the same trigger id, and
 * we've transitioned to the correct side.  Ponder what to do with it.
 */
MslResult* MslConductor::resume(MslContext* c, MslRequest* req, MslSession* session)
{
    MslResult* result = nullptr;

    MslSuspendState* susstate = session->getSustainState();
    
    if (req->release) {
        if (susstate->start == 0) {
            // session was not sustaining
            // this either means it wasn't sustainable, which should have been caught
            // earlier or it terminated during a context transition, which is odd
            // but possible?
            // is this an error, or silently ignore it?
            Trace(1, "MslConductor: Release request for non-sustaining session");
        }
        else {
            // todo: once this locates the session it would be good to verify that
            // the link->function matches the session, but it isn't really necessary
            // because triggers can only do one thing
            // !! well until the point where you allow multiple bindings on one trigger
            // if you allow that then several layers from here on down will need to
            // be more complicated than just searching by triggerId, will need to pass
            // the Linkage through as well
            result = release(c, req, session);
        }
    }
    else {
        if (susstate->start > 0) {
            // we got a retrigger for a session that is still waiting for
            // an up transition, we either missed the release request due to
            // an internal error, or the container isn't sending us good things
            // cancel the sustain
            Trace(1, "MslConductor: Retrigger of script waiting for release");
            susstate->init();
        }

        MslSuspendState* repstate = session->getRepeatState();
        if (repstate->start > 0) {
            // it is expecting a repeat trigger
            result = repeat(c, req, session);
        }
        else {
            // it is suspended for a reason other than a repeat
            // here is where we should check for concurrency enabled
            // before we launch a new one
            Trace(2, "MslConductor: Starting new concurrent session");
            result = start(c, req);
        }
    }
    return result;
}

/**
 * Start a new session
 */
MslResult* MslConductor::start(MslContext* c, MslRequest *req)
{
    MslResult* result = nullptr;

    // Environment should have caught this by now
    MslLinkage* link = req->linkage;
    if (link == nullptr) {
        Trace(1, "MslConductor: Request with no Linkage");
    }
    else {
        MslSession* session = environment->getPool()->allocSession();

        // nice for the monitor
        link->runCount++;
        
        session->start(c, link, req);

        result = checkCompletion(c, session);
    }
    return result;
}

/**
 * Here for a Request with a release action.
 * 
 * These make sense only for scripts that used #sustain which are normally still
 * waiting for the release.  Should have been verified by the caller.
 */
MslResult* MslConductor::release(MslContext* c, MslRequest* req, MslSession* session)
{
    MslResult* result = nullptr;
    
    // yes it is, let it go
    // we pass the Request in so OnRelease can have request arguments
    // rare but possible
    session->release(c, req);

    // the statin after this is normally not active, should we force it?
    MslSuspendState* state = session->getSustainState();
    if (state->isActive()) {
        Trace(1, "MslConductor: Lingering sustain state after release");
        state->init();
    }
    result = checkCompletion(c, session);
    
    return result;
}

/**
 * Cause an OnRepeat notification after checking that the request
 * does in fact mean a repeat rather than just a simple start.
 */
MslResult* MslConductor::repeat(MslContext* c, MslRequest* req, MslSession* session)
{
    MslResult* result = nullptr;
    
    // we pass the Request in so OnRelease can have request arguments
    // rare but possible
    session->repeat(c, req);

    // these normally don't complete until the timeout
    // unless there is a maxRepeat set
    result = checkCompletion(c, session);

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

/**
 * Send a request notification to the other side.
 * This has evolved to be the only NotificationFunction so revisit the
 * need for that.
 */
void MslConductor::sendRequest(MslContext* c, MslRequest* req)
{
    MslMessage* msg = messagePool.newMessage();

    msg->type = MslMessage::MsgRequest;

    // copy this for the other side
    msg->request.transfer(req);

    sendMessage(c, msg);
}

//////////////////////////////////////////////////////////////////////
//
// Wait/Form Resume
//
//////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////
//
// Old Diagnostics
//
//////////////////////////////////////////////////////////////////////

#if 0
/**
 * Attempt to correct damanage to the process list.
 * Got here during some script testing and not sure why.  I think it was related
 * to missing the release transition of a sustain script.  This is just for
 * debugging, it shouldn't happen normally.
 */
void MslConductor::repairProcesses(MslContext* c)
{
    (void)c;
    // this is all very dangerous and should be csect protected, but it's
    // temporary diagnostics

    for (auto process : processes) {

        MslSession* s = findSession(shellSessions, process);
        if (s != nullptr) {
            if (process->context != MslContextShell)
              Trace(1, "MslConductor: Found process with mismatched context");
        }
        else {
            MslSession* s = findSession(kernelSessions, process);
            if (s != nullptr) {
                if (process->context != MslContextKernel)
                  Trace(1, "MslConductor: Found process with mismatched context");
            }
            else {
                Trace(1, "MslConductor: Found process with no session");
            }
        }
    }
}

MslSession* MslConductor::findSession(MslSession* list, MslProcess* p)
{
    MslSession* found = nullptr;
    while (list != nullptr) {
        if (list->process == nullptr) {
            Trace(1, "MslConductor: Session without a process");
        }
        else if (list->process == p) {
            found = list;
            break;
        }
        list = list->next;
    }
    return found;
}
#endif

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
