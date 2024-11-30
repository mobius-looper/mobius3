/**
 * The MSL interpreter.
 *
 * Interpretation of symbols has been broken out into the MslSymbol file.
 *
 */

#include <JuceHeader.h>

#include "../util/Util.h"
#include "../model/UIAction.h"
#include "../model/UIParameter.h"
#include "../model/Query.h"
#include "../model/Symbol.h"
#include "../model/ScriptProperties.h"
#include "../Supervisor.h"

#include "MslError.h"
#include "MslModel.h"
#include "MslSymbol.h"
#include "MslDetails.h"
#include "MslStack.h"
#include "MslBinding.h"
#include "MslExternal.h"
#include "MslCompilation.h"
#include "MslEnvironment.h"

#include "MslSession.h"

MslSession::MslSession(MslEnvironment* env)
{
    environment = env;
    // need this all the time so cache it here
    pool = env->getPool();

    // should be large enough to handle most track counts
    // without needing to allocate if we're in the kernel
    scopeExpansion.ensureStorageAllocated(64);
}

/**
 * These are normally only deleted when the MslPool destructs and
 * should be clean.  
 * 
 * But if we land in an error condition trying to clean up, delete things
 * rather than returning them to the pool.  The pool should still be valid
 * at this point, but it is sensitive to static initialization order
 * in supervisor and I'd like to know if we get here because it means something
 * isn't pooling properly.
 */
MslSession::~MslSession()
{
    if (stack != nullptr) {
        Trace(1, "MslSession: You're deleting a loaded session without freeing it to the pool");
        while (stack != nullptr) {
            MslStack* prev = stack->parent;
            delete stack;
            stack = prev;
        }
    }

    if (rootValue != nullptr) {
        // this one does cascade
        Trace(1, "MslSession: Lingering rootValue on delete");
        delete rootValue;
    }
    
    if (errors != nullptr) {
        // this one does cascade
        Trace(1, "MslSession: Lingering errors on delete");
        delete errors;
    }
}

void MslSession::init()
{
    next = nullptr;
    result = nullptr;
    sessionId = 0;
    process = nullptr;

    unit = nullptr;
    context = nullptr;
    stack = nullptr;
    transitioning = false;
    
    triggerId = 0;
    sustaining.init();
    repeating.init();
    
    errors = nullptr;
    rootValue = nullptr;
    trace = false;
}

void MslSession::setTrace(bool b)
{
    trace = b;
}

bool MslSession::isTrace()
{
    return trace;
}

StructureDumper& MslSession::getLog()
{
    return log;
}

/**
 * Return true if we're transitioning.
 * This is just a flag that will toss the session to the other side.
 * It is important that the session logic does not flag a transition that doesn't
 * make sense, or else it will just keep bouncing between them.
 */
bool MslSession::isTransitioning()
{
    return (transitioning == true);
}

/**
 * Return true if we're waiting on something.
 * This is not normally true if transitioning is also true since
 * we shold do the transition before we enter the wait state.
 *
 * When we get to the point where sessions can have multiple threads of
 * execution will have to check all threads.
 *
 * Currently a wait state is indiciated by the top of the stack having
 * an active MslWait 
 */
bool MslSession::isWaiting()
{
    bool waiting = (stack != nullptr && stack->wait.active);
    
    if (waiting && transitioning)
      Trace(1, "MslSession: I'm both transitioning and waiting, can this happen?");
    
    return waiting;
}

bool MslSession::isSuspended()
{
    return (sustaining.isActive() || repeating.isActive());
}

MslSuspendState* MslSession::getSustainState()
{
    return &sustaining;
}

MslSuspendState* MslSession::getRepeatState()
{
    return &repeating;
}

/**
 * Only for MslScriptlet and the console
 */
MslWait* MslSession::getWait()
{
    MslWait* wait = nullptr;
    if (stack != nullptr && stack->wait.active)
      wait = &(stack->wait);
    return wait;
}

/**
 * Return true if the script has finished.
 * This is indicated by an empty stack or the presence of errors.
 * The error list is what distinguishes this from simply testing
 * !transitioning and !waiting.  We can be in either of those states
 * but are still fished because of errors.
 */
bool MslSession::isFinished()
{
    return (stack == nullptr || hasErrors());
}

bool MslSession::hasErrors()
{
    return (errors != nullptr);
}

/**
 * Name to use in the MslResult and for logging.
 */
const char* MslSession::getName()
{
    const char* s = nullptr;
    if (unit != nullptr)
      s = unit->name.toUTF8();
    return s;
}

//////////////////////////////////////////////////////////////////////
//
// Start/Resume
//
//////////////////////////////////////////////////////////////////////

/**
 * Primary entry point for evaluating a script.
 */
void MslSession::start(MslContext* argContext, MslCompilation* argUnit,
                       MslFunction* argFunction, MslRequest* request)
{
    prepareStart(argContext, argUnit);

    stack = pool->allocStack();
    stack->node = argFunction->getBody();
    stack->bindings = gatherStartBindings(argUnit, argFunction, request);
    
    run();

    checkSustainStart();
    checkRepeatStart();
}

void MslSession::prepareStart(MslContext* argContext, MslCompilation* argUnit)
{
    // should be clean out of the pool but make sure
    pool->free(errors);
    errors = nullptr;
    pool->free(rootValue);
    rootValue = nullptr;
    pool->freeList(stack);
    stack = nullptr;
    
    context = argContext;
    unit = argUnit;

    log.clear();
    logStart();
}

/**
 * At the end of each start() check to see if this is a #repeat script
 * and prepare it for suspension.
 */
void MslSession::checkRepeatStart()
{
    if (unit->repeat) {
        int timeout = unit->repeatTimeout;
        if (timeout == 0)
          // need a configuurable default
          timeout = 1000;
        repeating.activate(timeout);
    }
}

/**
 * At the end of each start() or repeat() check to see if we need to sustain
 */
void MslSession::checkSustainStart()
{
    if (unit->sustain) {
        int timeout = unit->sustainInterval;
        if (timeout == 0)
          // need a configuurable default
          timeout = 1000;
        sustaining.activate(timeout);
    }
}

/**
 * Resume a script after transitioning or to check wait states.
 * If we transitioned, it will just continue from the previous node.
 * If waiting, we'll immediately wait again unless the MslWait was
 * modified.
 *
 * If this was transitioning, the transition has happened, and that
 * state can be cleared.
 * 
 * Everything else is left untouched, the error list may be non-empty
 * if we're transitioning from the kernel back to the shell to show results.
 */
void MslSession::resume(MslContext* argContext)
{
    transitioning = false;

    // stack and bindings remain in place
    context = argContext;

    logContext("resume", argContext);

    // run may just immediately return if there were errors
    // or if the MslWait hasn't been satisified
    run();

    // if we have either a pending sustain release or a repeat timeout
    // do them now
    // todo: need to think about how sustan/repeat work when the
    // script is waiting, we could push a new stack frame just for the notification
    // rather than doing it after the wait and the script finishes
    // but this complicates how the start bindings work for the notification
    if (stack == nullptr) {
        if (sustaining.pending)
          release(context, nullptr);
        
        if (repeating.pending)
          repeat(context, nullptr);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Sustain/Repeat Notifications
//
//////////////////////////////////////////////////////////////////////

/**
 * Here if Environment has received the up transition for a sustaining session.
 */
void MslSession::release(MslContext* argContext, MslRequest* request)
{
    if (!unit->sustain) {
        // shouldn't have allowed this
        Trace(1, "MslSession::release Script was not sustainable");
        sustaining.init();
    }
    else if (!sustaining.isActive()) {
        // also should not be here
        // it is at least a #susstain script so we could call OnRelease
        // but don't lead them into unpredictable expectations
        Trace(1, "MslSession::release Script was not sustaining");
        sustaining.init();
    }
    else if (stack != nullptr) {
        // the script is still waiting on something
        // either a wait or a transition, or is just being slow?
        // defer the notification until it resumes
        Trace(2, "MslSession::release Script was busy");
        sustaining.pending = true;
    }
    else {
        // if request is nullptr here, it means that the session was busy or in the
        // opposite context when the resume event was received
        // we can resume it now but the request arguments have been lost
        // need a thread safe way of saving those in the suspension state
        MslNode* node = getNotificationNode(MslNotificationRelease);
        if (node != nullptr)
          runNotification(argContext, request, node);
        else {
            // else they weren't interested which is unusual if they bothered with #sustain
            // but allowed
            Trace(2, "MslSession::release No OnRelease function");
        }
        sustaining.init();
    }
}

/**
 * Here when Environment detects that the sustain timeout has been reached
 * The count has already been advanced and it will be re-armed if the sustain
 * is still active when this returns.
 */
void MslSession::sustain(MslContext* argContext)
{
    if (!unit->sustain) {
        // shouldn't have allowed this
        Trace(1, "MslSession::sustain Script was not sustainable");
        sustaining.init();
    }
    else if (!sustaining.isActive()) {
        Trace(1, "MslSession::sustain Script was not sustaining");
        sustaining.init();
    }
    else if (stack != nullptr) {
        // script is waiting on something or is transitioning
        // it should only be a wait, if it was transitioning then it would have been picked
        // up by the maintenance cycle in the right context by now
        // unclear what to do, so ignore it and wait for the next timeout
        // ambiguity over the pending flag which is also used for release()
        // could have another flag?
        Trace(2, "MslSession::sustain Script was busy");

        // todo: it wouldn't be that hard to do the notification anyway by pushing
        // a new node and then returning to the currrent one
    }
    else {
        MslNode* node = getNotificationNode(MslNotificationSustain);
        if (node != nullptr)
          runNotification(argContext, nullptr, node);
        else {
            // it's okay, but unusual
            // why would you ask for #sustain if you dodn't provide a function?
            Trace(2, "MslSession::sustain No OnSustain function");
        }
    }
}

/**
 * Here when Environment gets a trigger down and the script is #repeat
 */
void MslSession::repeat(MslContext* argContext, MslRequest* request)
{
    if (!unit->repeat) {
        Trace(1, "MslSession::repeat Script was not repeeatable");
        repeating.init();
    }
    else if (!repeating.isActive()) {
        Trace(1, "MslSession::repeat Script was not waiting for repeats");
        repeating.init();
    }
    else if (stack != nullptr) {
        // the script is still waiting on something
        Trace(2, "MslSession::repeat Script was busy");
        // I guess reset the timer but DO NOT bump the counter
        // since we didn't call it?
        repeating.timeoutStart =  juce::Time::getMillisecondCounter();
        // amgiugity over the pending flag for both repeat() and timeout()
        // I think it makes sense to just ignore repeats if the script is waiting
    }
    else {
        // bump the counter before calling the notificcation function
        repeating.advance();
        MslNode* node = getNotificationNode(MslNotificationRepeat);
        if (node != nullptr)
          runNotification(argContext, request, node);
        else {
            // it's okay, but unusual
            Trace(2, "MslSession::repeat No OnRepeat function");
        }

        // sustain state may activate on repeats too
        checkSustainStart();
    }
}

/**
 * Here when Environment determined that the repeat timeout has been reached.
 */
void MslSession::timeout(MslContext* argContext)
{
    if (!unit->repeat) {
        Trace(1, "MslSession::timeout Script was not repeeatable");
        repeating.init();
    }
    else if (!repeating.isActive()) {
        Trace(1, "MslSession::timeout Script was not waiting for repeats");
        repeating.init();
    }
    else if (stack != nullptr) {
        // the script is still waiting on something
        
        // it could be a wait or a transition
        // like sustain, the issue is whether to set a pending flag and do it immediately
        // after the wait finishes or just let them accumulate
        Trace(2, "MslSession::sustain Script was busy");
        // so we could keep resetting the timtout until the script is no
        // longer busy, but it seems more natural just to end the repeat now
        repeating.pending;
    }
    else {
        MslNode* node = getNotificationNode(MslNotificationTimeout);
        if (node != nullptr)
          runNotification(argContext, nullptr, node);
        else {
            // it's okay and common
            Trace(2, "MslSession::timeout No OnTimeout function");
        }
        repeating.init();
    }
}

MslNode* MslSession::getNotificationNode(MslNotificationFunction func)
{
    MslNode* node = nullptr;
    
    // determine the name of the function to call
    // should be able to override the deafult names if desired
    const char* name = nullptr;
    switch (func) {
        case MslNotificationSustain: name = "OnSustain"; break;
        case MslNotificationRepeat: name = "OnRepeat"; break;
        case MslNotificationRelease: name = "OnRelease"; break;
        case MslNotificationTimeout: name = "OnTimeout"; break;
    }

    node = findNotificationFunction(name);
    return node;
}

/**
 * Only supporting notification functions on the root unit.
 * If the script started from a Function instead, would need to look
 * for inner functions inside that one.
 *
 * We don't need to handle this like a normal function call.
 * The "arguments" to the function have already been effectively left
 * as bindings on the root frame and notification functions don't need to support
 * keyword arguments or optionals or anything fancy.
 * Just return the function nodes body block.
 */
MslNode* MslSession::findNotificationFunction(const char* name)
{
    MslNode* node = nullptr;
    
    MslFunction* body = unit->getBodyFunction();
    if (body == nullptr) {
        Trace(1, "MslSession::findFunction Unit with no body");
    }
    else {
        MslBlock* block = body->getBody();
        MslFunctionNode* found = nullptr;
        for (auto child : block->children) {
            MslFunctionNode* fnode = child->getFunction();
            if (fnode != nullptr && fnode->name == juce::String(name)) {
                found = fnode;
                break;
            }
        }

        if (found != nullptr)
          node = found->getBody();
    }
    return node;
}

/**
 * Run one of the notification functions
 */
void MslSession::runNotification(MslContext* argContext, MslRequest* request, MslNode* node)
{
    prepareStart(argContext, unit);
    
    stack = pool->allocStack();
    stack->node = node;

    // restore static bindings
    MslBinding* bindings = gatherStartBindings(unit, nullptr, request);
    
    // add or update the suspension state arguments
    bindings = addSuspensionBindings(bindings);
    
    stack->bindings = bindings;
    
    run();
}

/**
 * Add suspension state bindings to the list of start bindings
 * when running a notification function.
 *
 * Note that this sets the "transient" flag on the bindings so they
 * will be filtered out by saveStaticBindings when the script ends.
 * If you don't do this they'll accumulate on each notification.
 */
MslBinding* MslSession::addSuspensionBindings(MslBinding* start)
{
    MslBinding* combined = start;
    
    if (sustaining.isActive()) {
        MslBinding* b = makeSuspensionBinding("sustainCount", sustaining.count);
        b->next = combined;
        combined = b;

        // what is more useful, knowing the start or total elapsed?
        int now = juce::Time::getMillisecondCounter();
        int elapsed = now - sustaining.start;
        b = makeSuspensionBinding("sustainElapsed", elapsed);
        b->next = combined;
        combined = b;
    }

    if (repeating.isActive()) {
        MslBinding* b = makeSuspensionBinding("repeatCount", repeating.count);
        b->next = combined;
        combined = b;

        // any use for passing the elapsed time like sustain?
    }
    
    return combined;
}

MslBinding* MslSession::makeSuspensionBinding(const char* name, int value)
{
    MslBinding* b = pool->allocBinding();
    b->setName(name);
    MslValue* v = pool->allocValue();
    v->setInt(value);
    b->value = v;
    // important for filtering at the end!
    b->transient = true;
    return b;
}

//////////////////////////////////////////////////////////////////////
//
// StartEnd Bindings
//
//////////////////////////////////////////////////////////////////////

/**
 * Assemble the initial bindings for the root block before running.
 *
 * This includes the "static" bindings that are left on the compilation unit,
 * as well as arguments passed in the Request.
 *
 * !! todo: potential thread issues
 * It is rare to have concurrent evaluations of the same script and still
 * rarer to be doing it from both the UI thread and the audio thread at the same time
 * but it can happen.
 *
 * If static bindings are maintained on the MslCompilation and copied to the MslStack,
 * any new values won't be "seen" by other threads until this session ends.
 * Also, the last thread to execute overwrite the values of the previous ones
 * To do this the Right Way, static variable bindings need to be more like exported
 * bindings and managed in a single shared location with csects around access.
 *
 * Request arguments can be passed as a list of MslValue positional arguments
 * or as a list of MslBinding named arguments.  I started with just positional
 * but moved to named bindings because it's better for system scripts.
 *
 * Should not have both, but in theory this should assemble them like we do
 * for a combination of positional and keyword arguments passed in a function call.
 */
MslBinding* MslSession::gatherStartBindings(MslCompilation* argUnit,
                                            MslFunction* argFunction,
                                            MslRequest* request)
{
    // in theory we should use this to get to a signature to assist binding compilation
    (void)argFunction;
    
    MslBinding* startBindings = nullptr;

    // While findBinding will look up into the unit if it doesn't find anything on the stack,
    // copying it onto the root stack frame reduces thread contention since modificiations
    // made during this session will be held locally until the script completes, and then
    // copied back into the unit.
    startBindings = argUnit->bindings;
    argUnit->bindings = nullptr;

    // add request arguments
    if (request != nullptr) {
        int position = 1;

        // old way, positional
        if (request->arguments != nullptr) {
            MslValue* arg = request->arguments;
            while (arg != nullptr) {
                MslValue* nextv = arg->next;
                arg->next = nullptr;
                
                MslBinding* b = pool->allocBinding();
                b->value = arg;
                b->position = position;
                b->next = startBindings;
                startBindings = b;
                position++;
                arg = nextv;
            }
            // ownership was taken
            request->arguments = nullptr;
        }

        // new way, pass named bindings rather than positionals
        if (request->bindings != nullptr) {
            MslBinding* binding = request->bindings;
            while (binding != nullptr) {
                MslBinding* nextb = binding->next;
                binding->next = startBindings;
                startBindings = binding;
                binding->position = position;
                position++;
                binding = nextb;
            }
            // ownership was taken
            request->bindings = nullptr;
        }
    }

    logBindings("gatherStartBindings", startBindings);
    
    return startBindings;
}

/**
 * When evaluation has run to completion, move the "static" bindings that
 * were copied to the root stack by gatherStartBindings back to the unit.
 *
 * todo: technically should only be doing this for static variables
 * and not random locals declared at the top level.
 * Or we could have the binding reset when the MslVariable that defines
 * them is encountered during evaluation.
 *
 * What we DO need to do is filter out request arguments that were added
 * by gatherStartBindings, these will have a non-zero position.
 *
 * Also filter out bindings added for sustain/repeat state.
 * These will have the transient flag set on them, which could be useful
 * as well for filtering non-static local bindings.
 */
void MslSession::saveStaticBindings()
{
    MslBinding* finalBindings = stack->bindings;
    stack->bindings = nullptr;

    MslBinding* b = finalBindings;
    MslBinding* prev = nullptr;
    while (b != nullptr) {
        MslBinding* nextb = b->next;
        if (b->position > 0 || b->transient) {
            // it was a request argument or other non-static binding
            if (prev == nullptr)
              finalBindings = nextb;
            else
              prev->next = nextb;
            b->next = nullptr;
            pool->free(b);
        }
        else {
            prev = b;
        }
        b = nextb;
    }

    logBindings("finalBindings", finalBindings);
        
    unit->bindings = finalBindings;
}

//////////////////////////////////////////////////////////////////////
//
// Results
//
//////////////////////////////////////////////////////////////////////

/**
 * Ownership of the result value DOES NOT TRANSFER to the caller.
 * Who uses this?  Anything that cares should be using captureValue
 */
MslValue* MslSession::getValue()
{
    return rootValue;
}

/**
 * Transfer ownership of the final result.
 */
MslValue* MslSession::captureValue()
{
    MslValue* retval = rootValue;
    rootValue = nullptr;
    return retval;
}

MslError* MslSession::getErrors()
{
    return errors;
}

MslError* MslSession::captureErrors()
{
    MslError* retval = errors;
    errors = nullptr;
    return retval;
}

/**
 * Called internally to add a runtime error.
 * Using MslError here so we can capture the location in the source
 * of the node having issues, but unfortunately the parser isn't leaving
 * that behind yet.
 */
void MslSession::addError(MslNode* node, const char* details)
{
    MslError* e = pool->allocError();
    e->init(node, details);
    e->next = errors;
    errors = e;
}

//////////////////////////////////////////////////////////////////////
//
// Run Loop
//
//////////////////////////////////////////////////////////////////////

/**
 * The run loop processes the node at the top of the stack until
 * all stack frames have been consumed, a wait state has been reached,
 * or an unrecoverable runtime error has been encountered.
 */
void MslSession::run()
{
    //if (stack != nullptr)
    //aTrace(2, "Run: %s", debugNode(stack->node).toUTF8());
    
    while (stack != nullptr && errors == nullptr && !transitioning && !isWaitActive()) {
        advanceStack();
    }
}

/**
 * This is called by the run loop to determine if the stack is active and not
 * finished wait.  This differs from isWaiting for non-obvious reasons, I think
 * because isWaiting is used for initial results, and here we need to adapt to
 * the completion of a wait asynchronously.
 *
 * I dislike having the logic duplicated here and in the MslWaitNode visitor but
 * we've got a control flow issue.  If we use only wait.active in the run loop then
 * MslWaitNode will never get processed, but if we don't check wait.active we'll never
 * stop.  Might be better to have a "newWait" flag that is used like "transitioning" and can
 * be cleared every time we're resumed, then set again if WaitNode decides it wasn't finished.
 * Will have to do that eventually when you get to multiple threads.  Advance all threads
 * then test of isAnyWaiting or something.
 */
bool MslSession::isWaitActive()
{
    return (stack->wait.active && !stack->wait.finished);
}

/**
 * During an advance, a handler for the node associated with the frame is called.
 * This dispatching is done through a "visitor" pattern using overloaded class methods
 * to handle the dispatch and downcasting.
 *
 * The node handler performs actions and calculates a result value.  Then
 * the stack is popped and the result value is added to the child results list
 * of the previous stack frame.
 *
 * Note node handlers will usually push additional frames onto the stack.
 * These frames are then evaluated, results accumulated, and a final
 * aggregate result is calculated and returned.  In this process a stack frame
 * may be advanced several times, each time accumulating more information from
 * the child nodes until a completion state is reached.
 */
void MslSession::advanceStack()
{
    if (stack->node != nullptr) {
        // warp to a node-specific handler
        stack->node->visit(this);
    }
    else {
        // here is where we might add special stack frames that are not
        // associated with language nodes?
    }
}

/**
 * Called by node handlers to push a new frame onto the stack.
 * The stack frame is returned and in some cases may be further initialized
 * by the node handler.
 */
MslStack* MslSession::pushStack(MslNode* node)
{
    logNode("pushStack", node);
    MslStack* neu = pool->allocStack();

    neu->node = node;
    neu->parent = stack;
    
    stack = neu;

    //Trace(2, "Pushed: %s", debugNode(stack->node).toUTF8());
    return neu;
}

/**
 * Called by node handlers to push the next child node onto the stack.
 * Most nodes do simple iteration of their child nodes in order.
 * If we run out of children, nullptr is returned.
 */
MslStack* MslSession::pushNextChild()
{
    MslStack* neu = nullptr;

    // todo: some nodes will want more control over this list
    juce::OwnedArray<MslNode>* childNodes = &(stack->node->children);

    // this starts at -1 
    stack->childIndex++;
    if (stack->childIndex < childNodes->size()) {
        MslNode* nextNode = (*childNodes)[stack->childIndex];
        neu = pushStack(nextNode);
    }
    return neu;
}

/**
 * When a frame has completed, transfer a calculated value to the parent frame,
 * remove the current frame from the stack, and return control to the parent frame.
 *
 * There are two ways values can be returned from a child frame to the parent.
 * Simple nodes have single-valued results, prior results if any are discarded and
 * the new result is saved.
 *
 * A frame may also be designated as an "accumulator" frame.  Here, the values from
 * every child frame are accumulated on a list where they can be processed in bulk
 * by the parent frame.
 *
 * There is a bit of ugliness here when dealing with the results for the root frame.
 * Since there is no parent frame, the results to in the rootValue list managed
 * by the session.  Root results are always accumulated.  It would simplify some things
 * if we always had a dummy block at the top of the stack to accumulate root results.
 */
void MslSession::popStack(MslValue* v)
{
    logPop(v);
    // it is permissible to pop without a value, if the child wants nullness to have
    // meaning, it must return an empty MslValue
    MslStack* parent = stack->parent;
    if (parent == nullptr) {
        // we're at the root frame
        if (rootValue == nullptr)
          rootValue = v;
        else {
            MslValue* last = rootValue->getLast();
            last->next = v;
        }

        // save the final bindings for static variables back to the compilation unit
        saveStaticBindings();

        logLine("Finished");
    }
    else if (!parent->accumulator) {
        // replace the last value
        pool->free(parent->childResults);
        parent->childResults = v;
    }
    else if (parent->childResults == nullptr) {
        // we're an accumulator, and this is the first one
        parent->childResults = v;
    }
    else {
        // add the result to the end of the list
        MslValue* last = parent->childResults->getLast();
        last->next = v;
    }

    // now do the popping part
    MslStack* prev = stack->parent;
    pool->free(stack);
    stack = prev;
}

/**
 * Pop the stack and return whatever child results remain on the
 * current stack frame.
 */
void MslSession::popStack()
{
    // ownership transfers so must clear this before pooling
    MslValue* cresult = stack->childResults;
    stack->childResults = nullptr;
    
    popStack(cresult);
}
    
// need to beef this up
void MslSession::checkCycles(MslValue* v)
{
    if (found(v, v->next))
      Trace(1, "Cycle in next list");
    else if (found(v, v->list))
      Trace(1, "Cycle in value list");
}

bool MslSession::found(MslValue* node, MslValue* list)
 {
    bool found = false;
    MslValue* ptr = list;
    while (ptr != nullptr) {
        if (ptr == node) {
            found = true;
            break;
        }
        ptr = ptr->next;
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// Bindings
//
//////////////////////////////////////////////////////////////////////

/**
 * Walk up the stack looking for a binding, and finally into
 * the static binging list in the script.
 */
MslBinding* MslSession::findBinding(const char* name)
{
    MslBinding* found = nullptr;

    MslStack* level = stack;

    while (found == nullptr && level != nullptr) {

        if (level->bindings != nullptr)
          found = level->bindings->find(name);

        if (found == nullptr)
          level = level->parent;
    }
    
    // if we make it to the top of the stack, look in the static bindings
    // !! potential thread issues here
    if (found == nullptr) {
        if (unit->bindings != nullptr)
          found = unit->bindings->find(name);
    }

    return found;
}

MslBinding* MslSession::findBinding(int position)
{
    MslBinding* found = nullptr;

    MslStack* level = stack;
    while (found == nullptr && level != nullptr) {
        if (level->bindings != nullptr)
          found = level->bindings->find(position);

        if (found == nullptr)
          level = level->parent;
    }
    
    // if we make it to the top of the stack, look in the static bindings
    // !! potential thread issues here
    if (found == nullptr) {
        if (unit->bindings != nullptr)
          found = unit->bindings->find(position);
    }

    return found;
}

/**
 * This is a user accessible function exposed through MslSessionInterface
 * which is returned in an MslAction.  The containing application may use this
 * to obtain the values of random script variables, similar to the way
 * internal functions can freely reference dynamically bound variables.
 *
 * The returned value is "live" and remains owned by the session and should
 * not be modified.  This just saves having to make a copy since the caller almost
 * never needs to retain one and has to remember to free it.  But it's dangerous
 * and can hose the session if they mishandle it so might want to copy it anyway.
 * Also consider having a set of getInt, getString functions that don't copy and
 * don't need to be freed but whose value is not guaraneteed to remain stable.
 */
MslValue* MslSession::getVariable(const char* name)
{
    MslValue* value = nullptr;

    MslBinding* binding = findBinding(name);
    if (binding != nullptr) {
        value = binding->value;
    }
    else {
        // MslSymbol evaluation will at this point look for an
        // MslResolution that may have an MslLinksge to an exported
        // variable from another script.  We can't do pre-resolution
        // but it's a small amount of overhead.
        MslLinkage* link = environment->find(juce::String(name));
        if (link != nullptr && link->variable != nullptr) {

            // see MslSession::returnVariable for why this isn't implemented yet
        }
    }
    

    return value;
}

//////////////////////////////////////////////////////////////////////
//
// Literal
//
//////////////////////////////////////////////////////////////////////

/**
 * Stack handler for a literal.
 * Literals do not have child nodes or computation, they simply return
 * their value to the parent frame.
 */
void MslSession::mslVisit(MslLiteral* lit)
{
    logVisit(lit);
    MslValue* v = pool->allocValue();
    
    if (lit->isInt) {
        v->setInt(atoi(lit->token.value.toUTF8()));
    }
    else if (lit->isBool) {
        // could be a bit more relaxed here but this is what the tokanizer left
        v->setBool(lit->token.value == "true");
    }
    else {
        v->setJString(lit->token.value);
    }

    popStack(v);
}

//////////////////////////////////////////////////////////////////////
//
// Block
//
//////////////////////////////////////////////////////////////////////

/**
 * Block frames evaluate each child in order and may or may not accumulate
 * results.  The accumulation flag will have been set by the parent frame
 * that pushed this one.
 */
void MslSession::mslVisit(MslBlock* block)
{
    logVisit(block);
    
    MslStack* nextStack = pushNextChild();
    if (nextStack == nullptr) {
        // ran out of children, at this point we return the aggregate result
        // to the parent
        // todo: this is where we may want to make the distinction between
        // concatenating a list onto the parent or returning a single
        // value that is itself a list, will become interesting if we ever
        // support arrays as values
        popStack();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Variable
//
//////////////////////////////////////////////////////////////////////

/**
 * When a var is encounterd, push the optional child node to calculate
 * an initial value.
 *
 * When the initializer has finished, place a Binding for this var and
 * it's value into the parent frame
 */
void MslSession::mslVisit(MslVariable* var)
{
    logVisit(var);
    // the parser should have only allowed one child, if there is more than
    // one we take the last value
    MslStack* nextStack = pushNextChild();
    if (nextStack == nullptr) {
        // initializer finished
    
        MslStack* parent = stack->parent;
        if (parent == nullptr) {
            // this shouldn't happen, there can be vars at the top level of a script
            // but the script body should have been surrounded by a containing block
            addError(var, "Variable encountered above root block");
        }
        else if (!parent->node->isBlock()) {
            // the var was inside something other than a {} block
            // the parser allows this but it's unclear what that should mean, what use
            // would a var be inside an expression for example?  I suppose we could allow
            // this, but flag it for now until we find a compelling use case
            addError(var, "Variable encountered in non-block container");
        }
        else {
            MslBinding* b = pool->allocBinding();
            b->setName(var->name.toUTF8());
            // value ownership transfers
            b->value = stack->childResults;
            stack->childResults = nullptr;
            
            parent->addBinding(b);

            // vars do not have values themselves
            popStack(nullptr);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Function
//
//////////////////////////////////////////////////////////////////////

/**
 * This is the DECLARATION of a function not a call.
 * Currently, functions are "sifted" by the parser and left on a special list
 * in the MslScript so we should not encounter them during evaluation.
 * If that ever changes, then you'll have to deal with them here.
 * In theory we could have scoped function definitions like we do for vars.
 */
void MslSession::mslVisit(MslFunctionNode* func)
{
    logVisit(func);
    addError(func, "Encountered unsifted Function");
}

//////////////////////////////////////////////////////////////////////
//
// Reference
//
//////////////////////////////////////////////////////////////////////

/**
 * A $x reference.
 * These aren't used much now that we have named references but could
 * still be useful for special symbol handling.
 *
 * If one of these is unresolved it's a little more serious than
 * an unresolved symbol since you can't use them for just the name.
 */
void MslSession::mslVisit(MslReference* ref)
{
    logVisit(ref);
    MslBinding* binding = nullptr;
    
    int position = atoi(ref->name.toUTF8());
    if (position > 0) {
        binding = findBinding(position);
    }
    else {
        // it's not uncommon to do $foo to reference a named binding
        // though it isn't necessary
        binding = findBinding(ref->name.toUTF8());
    }

    if (binding != nullptr) {
        returnBinding(binding);
    }
    else {
        // return null or error?
        //MslValue* v = pool->allocValue();
        //popStack(v);
        // the token in the error message will just say $ rather than
        // the name which is inconvenient
        addError(ref, "Unresolved reference");
        Trace(1, "Unresolved reference %s", ref->name.toUTF8());
    }
}

/**
 * Here for a symbol or reference that resolved to an dynamic binding on the stack.
 * The binding holds the value to be returned, but it must be copied.
 */
void MslSession::returnBinding(MslBinding* binding)
{
    MslValue* value = binding->value;
    MslValue* copy = nullptr;
    if (value == nullptr) {
        // binding without a value, should this be ignored or does
        // it hide other things?
        copy = pool->allocValue();
    }
    else {
        // bindings can be referenced multiple times, so need to copy
        copy = pool->allocValue();
        copy->copy(value);
    }
    popStack(copy);
}

//////////////////////////////////////////////////////////////////////
//
// Arguments
//
//////////////////////////////////////////////////////////////////////

/**
 * Locate the value of an operand.
 * Normally they will be atomic values under the stack's value list.
 * But in the case of blocks, may (always?) be contained in a list wrapper
 * that should only have one eleemnt.
 *
 * e.g.
 *
 *     1 + 2 will be [1,2]
 * but
 *     (1) + 2 will be [[1],2]
 *
 * verify this.  Either way it shouldn't matter.  If we do encounter
 * a multi-value block result, i suppose take the last value?  hmm,
 * run some tests.  Blocks always return lists now and it is up to the
 * parent node to decide whether to use all of the values or just the last one.
 */
MslValue* MslSession::getArgument(int index)
{
    MslValue* value = nullptr;

    MslValue* arguments = stack->childResults;

    if (arguments == nullptr) {
        // this is probably a bug, node must not have been evaluated
        addError(stack->node, "Missing arguments");
    }
    else {
        value = arguments->get(index);
        if (value != nullptr) {
            if (value->list != nullptr) {
                value = value->list->getLast();
            }
        }
    }

    return value;
}

//////////////////////////////////////////////////////////////////////
//
// Operators
//
//////////////////////////////////////////////////////////////////////

/**
 * An operator node will normally have two children, one for unary.
 *
 * Kind of forgot about pushNextChild while my mind was melting with MslSymbol,
 * revisit the complicated ones and see if that could be used there instead
 * of phase markers?
 */
void MslSession::mslVisit(MslOperator* opnode)
{
    logVisit(opnode);
    if (opnode->children.size() == 0) {
        addError(opnode, "Missing operands");
    } 
    else {
        // tell popStack that we want all child values
        stack->accumulator = true;
        
        MslStack* nextStack = pushNextChild();
        if (nextStack == nullptr) {
            // ran out of children, apply the operator
            doOperator(opnode);
        }
    }
}

/**
 * Be relaxed about types here.  The only things we care about really
 * are numeric values and enumeration symbols coerced from/to ordinals.
 * Would be nice to do enum wrapping, but I don't think that belongs here.
 *
 * For boolean comparisons, try to be smart about enumerated values from
 * UIParameters.  Those will be saved in the MslValue as an Enum with both
 * the ordinal and the symbolic name.
 *
 * Null is treated as zero numerically which might be bad.
 */
void MslSession::doOperator(MslOperator* opnode)
{
    MslValue* v = pool->allocValue();
    
    //MslOperators op = mapOperator(opnode->token.value);
    MslOperators op = opnode->opcode;

    if (op == MslUnknown) {
        addError(opnode, "Unknown operator");
    }
    else {
        // everything needs two operands except for !
        MslValue* value1 = getArgument(0);
        MslValue* value2 = nullptr;
        if (op != MslNot)
          value2 = getArgument(1);

        if (errors == nullptr) {
    
            switch (op) {
                case MslUnknown:
                    // already added an error
                    break;
                case MslPlus:
                    addTwoThings(value1, value2, v);
                    break;
                case MslMinus:
                    v->setInt(value1->getInt() - value2->getInt());
                    break;
                case MslMult:
                    v->setInt(value1->getInt() * value2->getInt());
                    break;
                case MslDiv: {
                    int divisor = value2->getInt();
                    if (divisor == 0) {
                        // we're obviously not going to throw if they made an error
                        v->setInt(0);
                        // should be a warning
                        Trace(1, "MslSession: divide by zero");
                        //addError(opnode, "Divide by zero");
                    }
                    else {
                        v->setInt(value1->getInt() / divisor);
                    }
                }
                    break;

                    // for direct comparison, be smarter about coercion
                    // = and == are the same right now, but that probably won't work
                case MslEq:
                    v->setBool(compare(value1, value2, true));
                    break;
                case MslDeq:
                    v->setBool(compare(value1, value2, true));
                    break;
            
                case MslNeq:
                    v->setBool(compare(value2, value2, false));
                    break;
            
                case MslGt:
                    v->setInt(value1->getInt() > value2->getInt());
                    break;
                case MslGte:
                    v->setBool(value1->getInt() >= value2->getInt());
                    break;
                case MslLt:
                    v->setBool(value1->getInt() < value2->getInt());
                    break;
                case MslLte:
                    v->setBool(value1->getInt() <= value2->getInt());
                    break;
                case MslNot:
                    // here we check to make sure the node only has one child
                    v->setBool(!(value1->getBool()));
                    break;
                case MslAnd:
                    // c++ won't evaluate the second arg if the first one is false
                    // msl doesn't do deferred evaluation so no, we don't
                    v->setBool(value1->getBool() && value2->getBool());
                    break;
                case MslOr:
                    // c++ won't evaluate the second arg if the first one is true
                    v->setBool(value1->getBool() || value2->getBool());
                    break;
            
                    // unclear about this, treat it as and
                    //case MslAmp:
                    //v->setInt(value1->getBool() && value2->getBool());
                    //break;
            }
        }
    }
    
    popStack(v);
}

/**
 * When using the + operator if either side is a String,
 * coerce the other side to a string and do concatenation.
 * Think: This may not be what you always want.  It isn't uncommon
 * for something to produce a string that looks like a number.  Especially
 * when dealing with MIDI Binding arguments which is just a string.
 * It is probably better to have an explicit string concatenation operator.
 */
void MslSession::addTwoThings(MslValue* v1, MslValue* v2, MslValue* res)
{
    if (v1->type == MslValue::String || v2->type == MslValue::String) {
        // if we flesh out string operations more, would be nice if MslValue
        // could do this sort of thing
        char merged[128];
        // hello darkness my old friend
        snprintf(merged, sizeof(merged), "%s%s", v1->getString(), v2->getString());
        res->setString(merged);
    }
    else {
        res->setInt(v1->getInt() + v2->getInt());
    }
}

/**
 * Semi-smart comparison that deals with strings and symbols.
 * For UIParameter symbols, this is a little complicated and relies on the
 * value of the parameter being stored in the MslValue as an Enum containing
 * both the ordinal integer and the symbolic name string.
 *
 * Nullness isn't really a thing yet, which could lead to some weird
 * comparisons with string coercion.
 *
 * Symbols that resolve to UIParameters is a little lose right now, but the main
 * thing that needs to be supported is this:
 *
 *      if quantize == loop
 *
 * While the true value of most parameters is an ordinal number, no one thinks of it
 * that way, they want to compare against a symbolic name.  In the above example
 * the lhs will be an Enum and the RHS will be a String holding the name of an
 * unresolved symbol.
 *
 * Use of unresolved symbols is a little weird, and prevents checking for errors
 * at the point of evaluation.  But the alternative is to intern a bunch of Symbols.
 */
bool MslSession::compare(MslValue* value1, MslValue* value2, bool equal)
{
    bool bresult = false;

    if (value1->type == MslValue::String || value2->type == MslValue::String) {
        // it doesn't really matter if one side is a symbol Enum, they
        // both compare as strings
        // numeric coersion to strings is a little weird, does that cause trouble?
        if (equal)
          bresult = StringEqualNoCase(value1->getString(), value2->getString());
        else
          bresult = !StringEqualNoCase(value1->getString(), value2->getString());
    }
    else {
        // simple integer comparison
        if (equal)
          bresult = (value1->getInt() == value2->getInt());
        else
          bresult = (value1->getInt() != value2->getInt());
    }
    
    return bresult;
}

//////////////////////////////////////////////////////////////////////
//
// Conditionals
//
//////////////////////////////////////////////////////////////////////

/**
 * If nodes have at least two child nodes: a condition and a truth block
 * They may have an optional else block.  There will only be two phases.
 * First the condition block is pushed and evaluated and based on that
 * either the truth or false block is pushed.
 */
void MslSession::mslVisit(MslIf* node)
{
    logVisit(node);
    if (stack->phase == 0) {
        // need the condition
        int nodes = node->children.size();
        if (nodes == 0) {
            // malformed no condition, ignore or error?
            addError(node, "If with no condition");
        }
        else if (nodes == 1) {
            addError(node, "If with no consequence");
        }
        else {
            stack->phase = 1;
            pushStack(node->children[0]);
        }
    }
    else if (stack->phase == 1) {
        // back from the conditional
        // what is truth?
        bool truth = false;
        if (stack->childResults != nullptr)
          truth = stack->childResults->getBool();

        stack->phase = 2;
        if (truth && node->children.size() > 1) {
            pushStack(node->children[1]);
        }
        else if (!truth && node->children.size() > 2) {
            pushStack(node->children[2]);
        }
        else {
            // if truth falls in the forest, does it make a return value?
            // probably should return null for accumulators
            popStack(pool->allocValue());
        }
    }
    else if (stack->phase == 2) {
        // back from the consequence
        popStack();
    }
}

/**
 * These could be collapsed by the parser since they do nothing but just act
 * as a placeholder node.
 */
void MslSession::mslVisit(MslElse* node)
{
    logVisit(node);
    MslStack* nextStack = pushNextChild();
    if (nextStack == nullptr)
      popStack();
}

//////////////////////////////////////////////////////////////////////
//
// Wait
//
//////////////////////////////////////////////////////////////////////

/**
 * Wait nodes control an extension of the MslStack object that has
 * information about what we are waiting on and when it has finished.
 * It is necessary to put this on the stack rather than inside the node
 * because the node can be shared by multiple sessions.
 * This is in the MslWait object embedded within the MslStack.
 *
 * Since ALL MslStacks will have an MslWait, I suppose that could be used
 * for other nodes besides MslWaitNode, but it isn't right now.
 *
 * When an MslStack has an active MslWait, the session (or thread within the session)
 * suspends until the wait state is cleared by the outside, or
 * it times out.  Timeout is not yet implemented.
 *
 * For MslWaitNode it is important to know if this is the first time this
 * is visited or the second time after the MslWait has been satisfied
 * so control must return here order to cancel the wait.  This makes MslWait
 * not usable for other nodes without additional work.
 *
 */
void MslSession::mslVisit(MslWaitNode* wait)
{
    logVisit(wait);
    if (stack->wait.active) {
        // we've been here before
        if (stack->wait.finished) {
            // back here after MslEnvironment::resume was called and the context
            // decided the wait was over
            // might want to have an interesting return value here

            // the assumption is that the session can't be resumed unless it is
            // in a context that is prepared to deal with it, I don't think we
            // can resume from the other side
            if (context->mslGetContextId() != MslContextKernel)
              Trace(1, "MslSession: Wait resumed outside of the kernel context");


            if (stack->wait.coreEventCanceled) {
                // this indiciates that the event was canceled rather than being
                // reached normally, might want options on how to handle this
                Trace(2, "MslSession: Wait event was canceled");
            }
            
            stack->wait.init();
            popStack();
        }
        else {
            // still waiting, leave the stack as it is
        }
    }
    else {
        // starting a wait for the first time
        if (wait->type == MslWaitNone) {
            // this should have failed parsing
            addError(wait, "Missing or invalid wait type");
        }
        else {
            // accumulate all results
            stack->accumulator = true;
            
            // evaluate the amount/number/repeat expressions
            MslStack* nextStack = pushNextChild();
            if (nextStack == nullptr) {
                // transition if we're not in kernel before setting up the wait event
                // but after we've checked for syntax errors and evaluated the
                // wait value
                if (context->mslGetContextId() != MslContextKernel)
                  transitioning = true;
                else
                  setupWait(wait);
            }
        }
    }
}

/**
 * Set up the MslWait on this stack frame to start the wait.
 *
 * If this is "wait last" and there is no asyncAction event from the
 * last external call, then do nothing.
 * 
 * Interesting fringe case: we're doing this after evaluation of any
 * wait arguments for count/number/duration and in theory those could
 * have performed an asyncAction.  If so, then we'll be waiting on that
 * rather than the real previous action.  I guess you get what you pay for
 * and you shouldn't have wait arguments on a "wait last" anyway
 * still we could have checked this earlier and not even bothered evaluating
 * the wait argument if there is nothing to wait for.
 */
void MslSession::setupWait(MslWaitNode* node)
{
    if (node->type == MslWaitLast && asyncAction.event == nullptr) {
        Trace(2, "MslSession: Wait last was not after an async action");
        stack->wait.init();
        popStack();
    }
    else {
        MslWait* wait = &(stack->wait);
    
        wait->type = node->type;
        // the association between child results and what they mean is a bit loose
        // but its up to the user to use value expressions that will have actually
        // left the necessary number of child results.  If not, mayhem ensues
        if (node->amountNodeIndex >= 0) {
            MslValue* v = getChildResult(node->amountNodeIndex);
            if (v == nullptr)
              addError(node, "Missing wait amount value");
            else
              wait->amount = v->getInt();
        }
        if (!hasErrors() && node->numberNodeIndex >= 0) {
            MslValue* v = getChildResult(node->numberNodeIndex);
            if (v == nullptr)
              addError(node, "Missing wait number value");
            else
              wait->number = v->getInt();
        }
        if (!hasErrors() && node->repeatNodeIndex >= 0) {
            MslValue* v = getChildResult(node->repeatNodeIndex);
            if (v == nullptr)
              addError(node, "Missing wait repeat value");
            else
              wait->repeats = v->getInt();
        }

        if (!hasErrors()) {
            wait->forceNext = node->next;
            if (node->type == MslWaitLast) {
                // this is how the core knows what to wait on
                // the MOS interpreter handled this in a completely obscure way
                wait->coreEvent = asyncAction.event;
            }

            // save where it came from, the session is necessary so MslEnvironment
            // knows which one to resume, I don't think the stack is, it just advances the session
            // and the wait will normally be the top of the stack
            wait->session = this;
            wait->stack = stack;
            // important to clear this before the mslWait call
            wait->finished = false;
        
            // ask the context to schedule something suitable to end the wait
            // the context is allowed to retain a pointer to the wait object, and
            // expected to set the finished flag when the wait is over
            // todo: would really like to be able to have the context include more
            // details about what was wrong with the wait request
            // since we're almost always in the kernel now, have memory issues
            MslContextError error;
            if (!context->mslWait(wait, &error)) {
                addError(node, "Unable to schedule wait state");
                if (error.hasError())
                  addError(node, error.error);
            }
            else if (wait->finished) {
                // this is a special case
                // since an undefined amount of time may have elapsed between the
                // last asyncAction and the wait statement, the event might be gone
                // by now, don't treat this case as an error, just move on
                // could also use this for time boundary waits, like "wait beat"
                // when we start exactly on a beat, but will need some options on that
                // since they might want to arm this for the next beat instead
                Trace(2, "MslSession: Wait last finished immediately");
                stack->wait.init();
                popStack();
            }
            else {
                // make it go, or rather stop
                wait->active = true;
            }
        }
    }
}

/**
 * Locate a child result by index.
 * This is awkward for MslWait since the value expressions can be in any
 * order.  Cumbersome with a linked list but there are only three.
 */
MslValue* MslSession::getChildResult(int index)
{
    MslValue* found = nullptr;
    MslValue* value = stack->childResults;
    int position = 0;
    while (value != nullptr) {
        if (position == index) {
            found = value;
            break;
        }
        value = value->next;
        position++;
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// In
//
//////////////////////////////////////////////////////////////////////

void MslSession::mslVisit(MslIn* innode)
{
    logVisit(innode);
    if (stack->phase == 0) {
        // the first child block will always be the sequnce injected
        // by the parser
        if (innode->children.size() < 1)
          addError(innode, "Missing sequence");
        if (innode->children.size() < 2)
          addError(innode, "Missing body");
        stack->phase = 1;
        pushNextChild();
        // the sequenece is an accumulator
        stack->accumulator = true;
    }
    else if (stack->phase == 1) {
        if (stack->childResults == nullptr) {
            addError(innode, "No targets to iterate");
        }
        else {
            // convert the child list to an iteration list
            // saved on the stack
            MslValue* inList = nullptr;
            MslValue* inLast = nullptr;
            // capture the child results and reset it to accumulate body results
            MslValue* cv = stack->childResults;
            while (cv != nullptr) {
                if (cv->type == MslValue::String) {
                    // accept a few keywords as shorthand
                    if (!expandInKeyword(cv)) {
                        addError(innode, "Unrecognized track sequence keyword");
                    }
                    else {
                        // result was left here
                        for (int i = 0 ; i < scopeExpansion.size() ; i++) {
                            int number = scopeExpansion[i];
                            MslValue* v = pool->allocValue();
                            v->setInt(number);
                            if (inLast != nullptr)
                              inLast->next = v;
                            else
                              inList = v;
                            inLast = v;
                        }
                    }
                }
                else if (cv->type != MslValue::Int) {
                    // error or just warn and move on
                    // since we're in the middle of the script it might
                    // be better to ignore and let it finish then present warnings
                    // rather than just cancel it abruptly?
                    // todo: it's actually not the innode that is the problem
                    // it was the child node that produced this value
                    addError(innode, "Sequence term did not evaluate to a number");
                }
                else {
                    MslValue* scopenum = pool->allocValue();
                    scopenum->setInt(cv->getInt());
                    if (inLast != nullptr)
                      inLast->next = scopenum;
                    else
                      inList = scopenum;
                    inLast = scopenum;
                }
                cv = cv->next;
            }

            if (inList != nullptr) {
                // reset child results and accumulate body results
                pool->free(stack->childResults);
                stack->childResults = nullptr;
                stack->childIndex = -1;

                // save iteration list on the stack
                stack->inList = inList;
                // start the iteration
                stack->inPtr = inList;
                // advance the phase and evaluate this node again
                stack->phase = 2;
                // set this if you want to accumulate the results of all the body blocks
                stack->accumulator = true;
            }
            else {
                // scope symbol did not result in any sequence
                // nothing happens, skip the body
                popStack(nullptr);
            }
        }
    }
    else if (stack->phase == 2) {
        // for each number in stack->inList call the body block
        if (stack->inPtr != nullptr) {
            int scopeNumber = stack->inPtr->getInt();
            Trace(2, "MslSession: In iteration %d", scopeNumber);
            stack->phase = 3;
            // add a referenceable binding for the scope number
            if (stack->bindings == nullptr) {
                stack->bindings = pool->allocBinding();
                stack->bindings->setName("scope");
                stack->bindings->value = pool->allocValue();
            }
            // to make use of this for echo REALLY need to support format
            // strings or string catenation 
            stack->bindings->value->setInt(scopeNumber);
            pushStack(stack->node->children[1]);
        }
    }
    else if (stack->phase == 3) {
        // back from a body call
        stack->inPtr = stack->inPtr->next;
        // go back to phase 2 and evaluate the next one
        stack->phase = 2;

        // pop if we ran off the list
        // in does not have a return value though I suppose we could gather them in
        // the child list
        if (stack->inPtr == nullptr) {
            // at this point we were accumulating body block results
            // what interesting thing is there to do with them?  Need better
            // support for arrays
            Trace(2, "MslSession: In results");
            for (MslValue* v = stack->childResults ; v != nullptr ; v = v->next)
              Trace(2, "  %s", v->getString());
            popStack(nullptr);
        }
    }
}

/**
 * Given a keyword token from the "in" sequence, ask the Context to expand
 * that out into a set of scope (track) numbers.
 *
 * Currently these will have to be coded as quoted strings since we don't yet
 * support unresolved symbol references in here.  Would really like :all
 * here, rather than "all" or some way to defer symbol resolution.  Really
 * need an MslValue of type Symbol.  OR let these be normal symbols that have
 * a special derived evaluation.  Kind of a statement specific binding.
 *
 *     in all
 *
 * "all" becomes an MslSymbol node as normal, but Linker recognizes this as a keyword
 * and sets a special resolution flag, kind of like we do for functionArgument.
 * Then findBinding needs to have similar awareness of context specific calculated
 * binding values.
 *
 * Some of these require insight into track state which can't be done reliably
 * if we are not in Kernel context without doing a Query.  I suppose we could force
 * a transition if that happens.
 *
 */
bool MslSession::expandInKeyword(MslValue* keyword)
{
    scopeExpansion.clearQuick();
    return context->mslExpandScopeKeyword(keyword->getString(), scopeExpansion);
}

/**
 * This behaves just like a block after parsing
 */
void MslSession::mslVisit(MslSequence* seq)
{
    logVisit(seq);
    MslStack* nextStack = pushNextChild();
    if (nextStack == nullptr) {
        popStack();
    }
}

//////////////////////////////////////////////////////////////////////
//
// Thread control, random
//
//////////////////////////////////////////////////////////////////////

/**
 * context "shell" | "kernel"
 *
 * This transitions the session to one side of the great divide.
 * Normally this happens automatcially as symbols or waits are reached
 * that must be handled in a specific context, but it can also be
 * forced for testing.
 *
 * Note that once the transitioning flag is set in this session it will
 * move to whatever the other side is relative to the current MslContext.
 * So don't set this unless the requested context is actually different
 * from where we already are, or else it may bounce back and forth
 * endlessly.
 */
void MslSession::mslVisit(MslContextNode* con)
{
    logVisit(con);
    if (con->shell) {
        // the shell was requested
        if (context->mslGetContextId() == MslContextKernel) {
            // and it is different from where we are
            transitioning = true;
        }
        // else ignore
    }
    else {
        // the kernel was requested
        if (context->mslGetContextId() == MslContextShell)
          transitioning = true;
    }
    
    popStack(nullptr);
}

/**
 * Keywords have no value
 *
 * !! I kind of wish they did.  For externals it's a convenient way
 * to pass symbols: MidiOut(:note, 0, 42) rather than ("note", 0, 42)
 *
 * Why would this also not be useful for enumerations?
 *
 *     if (mode == :Reset)
 *     if (quantize == :subcycle)
 *
 * The later could avoid the coercion shenangans we're doing now and would
 * be consistent with "quoted symbol".  Alternately promote symbol as a MslValue
 * type that just has the name.
 */
void MslSession::mslVisit(MslKeyword* key)
{
    logVisit(key);
    popStack();
}

/**
 * Won't see these but have to overload it for the visitor
 */
void MslSession::mslVisit(MslInitNode* init)
{
    logVisit(init);
    addError(init, "Encountered init mode in the main body");
}

void MslSession::mslVisit(MslTrace* node)
{
    logVisit(node);
    if (node->control) {
        trace = node->on;
        if (trace)
          Trace(2, "MslSession: Turning trace on");
        else
          Trace(2, "MslSession: Turning trace off");
        popStack(nullptr);
    }
    else {
        // can avoid all this if trace isn't even on
        if (trace) {
            // basically the same as MslPrint
            // shold have a single child block
            MslStack* nextStack = pushNextChild();
            if (nextStack != nullptr) {
                // capture all results in the block
                nextStack->accumulator = true;
            }
            else {
                if (stack->childResults != nullptr) {
                    // this could be long so might want to use juce::String here
                    // even though it can allocate memory in the audio thread
                    char buffer[1024];
                    strcpy(buffer, "");
                    for (MslValue* v = stack->childResults ; v != nullptr ; v = v->next) {
                        if (v != stack->childResults) AppendString(" ", buffer, sizeof(buffer));
                        AppendString(v->getString(), buffer, sizeof(buffer));
                    }
                    logLine(buffer);
                }
                // trace has no return value so we don't clutter up the console displaying it
                popStack(nullptr);
            }
        }
        else {
            popStack(nullptr);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// End
//
//////////////////////////////////////////////////////////////////////

/**
 * Todo: might be nice to have end return a value for the script?
 */
void MslSession::mslVisit(MslEnd* end)
{
    logVisit(end);
    MslValue* v = pool->allocValue();
    v->setString("end");
    popStack(v);
    while (stack != nullptr) popStack();
}

/**
 * There are three ways print could work when there is more than one child node.
 *
 *    1) call the context's mslPrint once for each value
 *    2) concatenate the value strings and make one mslPrint call
 *    3) pass mslPrint a list of MslValues rather than one C string
 *
 * For eventual console debugging, we're going to need to capture print output
 * and save it in the session's MslResult so an MslMessage pooled object would be
 * helpful to handle both the capturing of results and conveying them to the context
 * right away.
 *
 * MslPrint accepts a single child node so if you to print more than one thing you
 * have to use a () block.  To make it look more like Lisp (print a b c) we would
 * need to accept multiple nodes and require a delimiter "print a b c;"  Since it is
 * most common to use this with single strings, don't require a delimiter.
 */
void MslSession::mslVisit(MslPrint* echo)
{
    logVisit(echo);

    // shold have a single child block
    MslStack* nextStack = pushNextChild();
    if (nextStack != nullptr) {
        // capture all results in the block
        nextStack->accumulator = true;
    }
    else {
        if (stack->childResults != nullptr) {
            char buffer[1024];
            strcpy(buffer, "");
            for (MslValue* v = stack->childResults ; v != nullptr ; v = v->next) {
                if (v != stack->childResults) AppendString(" ", buffer, sizeof(buffer));
                AppendString(v->getString(), buffer, sizeof(buffer));
            }
            context->mslPrint(buffer);
        }
        // echo has no return value so we don't clutter up the console displaying it
        popStack(nullptr);
    }
}

//////////////////////////////////////////////////////////////////////
//
// Debug
//
//////////////////////////////////////////////////////////////////////

juce::String MslSession::debugNode(MslNode* n)
{
    juce::String s;
    debugNode(n, s);
    return s;
}

void MslSession::debugNode(MslNode* n, juce::String& s)
{
    if (n == nullptr)
      s += "null ";
    else if (n->isBlock())
      s += "block ";
    else if (n->isSymbol())
      s += "symbol ";
    else if (n->isVariable())
      s += "var ";
    else
      s += "??? ";

    if (n->children.size() > 0) {
        s += "[";
        for (auto child : n->children) {
            debugNode(child, s);
        }
        s += "]";
    }
}

void MslSession::logLine(const char* line)
{
    if (trace) {
        log.line(line);
    }
}

void MslSession::logStart()
{
    if (trace) {
        log.start("MslSession:start");
        log.add("name", unit->name);
        log.newline();
        logContext("start", context);
    }
}

void MslSession::logContext(const char* title, MslContext* c)
{
    if (trace) {
        MslContextId id = c->mslGetContextId();
        juce::String sid;
        switch (id) {
            case MslContextNone: sid = "none"; break;
            case MslContextKernel: sid = "kernel"; break;
            case MslContextShell: sid = "shell"; break;
        }
        log.start(title);
        log.add("contextId", sid);
        log.newline();
    }
}

void MslSession::logBindings(const char* title, MslBinding* list)
{
    if (trace && list != nullptr) {
        log.line(juce::String(title));
        log.inc();
        for (MslBinding* b = list ; b != nullptr ; b = b->next) {
            juce::String value;
            if (b->value != nullptr) value = juce::String(b->value->getString());
            log.line(juce::String(b->name), value);
        }
        log.dec();
    }
}

const char* MslSession::getLogName(MslNode* node)
{
    const char* name = node->getLogName();
    if (strcmp(name, "?") == 0) {
        // set breakpoint here
        int x = 0;
        (void)x;
    }
    return name;
}

void MslSession::logVisit(MslNode* node)
{
    if (trace) {
        log.start("visit");
        log.add("type", getLogName(node));
        log.newline();
    }
}

void MslSession::logNode(const char* title, MslNode* node)
{
    if (trace) {
        log.start(title);
        log.add("node", getLogName(node));
        log.newline();
    }
}

void MslSession::logPop(MslValue* v)
{
    if (trace) {
        log.start("popStack");
        log.add("node", getLogName(stack->node));
        if (v == nullptr)
          log.add("value", "null");
        else
          log.add("value", v->getString());
        log.newline();
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
