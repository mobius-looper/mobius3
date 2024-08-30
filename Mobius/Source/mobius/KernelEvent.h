/**
 * Model for passing a requests from the kernel up to the shell for processing
 * outside of the audio thread.
 *
 * Most of these are related to file handling for the test scripts.
 * A few like Echo, Message, and Alert are used in scripts to pass
 * information back up to the UI to display status to the user.
 *
 * None of these can be done in the audio thread because they requires
 * access to system resources that are not allowed in time critical
 * code.
 *
 * This differs from actions which always go from shell down to kernel.
 *
 * The name "event" is a bit unfortunate since we also have the Event class
 * which is used for scheduling things to happen on the track timeline.
 *
 * This takes the place of what the old code called ThreadEvent.
 */

#pragma once

/**
 * The types of KernelEvents that demand attention.
 */
typedef enum {

    EventNone = 0,

    // wait for what?
    EventWait,

    // test script wants to save the current "playback audio"
    // captures an Audio object and forwards to MobiusListener::mobiuSaveAudio
    EventSaveLoop,

    // test script wants to save a capture file
    // this can also be caused by the SaveCapture function which
    // is a normal bindable function
    // forwards to MobiusListener::mobiusSaveCapture
    EventSaveCapture,
    
    // save an assembled project
    // this isn't implemented yet, and needs redesign
    EventSaveProject,
    
    // this was a weird one, it was in response to the UI setting OperatorPermanent
    // on a Setup action to cause it to be saved permanently in mobius.xml
    // we shouldn't need that in an Action handler, just do it in the UI if that's
    // what you want
    EventSaveConfig,
    
    // test script wants to load a loop
    // forwards to MobiusListener::mobiusLoadLoop
    EventLoadLoop,

    // test script wants to know "what's the difference, man"?
    // forwards to MobiusListener::mobiusDiff
    EventDiff,

    // get rid of this
    EventDiffAudio,

    // test script wants to ask the user a question
    // forwards to MobiusListener::mobiusPrompt
    EventPrompt,

    // Sent by ScriptEchoStatement
    // This is intended for debugging information that by default
    // goes to the Trace log, but may now also be displayed by TestDriver
    // Echo message are not intended for display in the normal UI
    // forwards to MobiusListener::mobiusEcho
    // todo: Since this exists only for TestDriver, we could avoid the
    // layer transitions and the ordering problems with KernelEvent by
    // just allowing these to be given to TestDriver directly from the audio
    // thread where it would queue them and display them on the next
    // maintenance cycle
    EventEcho,

    // Sent by ScriptMessageStatement
    // Intended for informational messages from scripts that are
    // visible to the user in Message element
    // forwards to MobiusInterface::mobiusMessage
    EventMessage,

    // Sent by the Alert function, which is mostly used in scripts, but
    // also in a few places in core code.
    // Intended for serious problems in the engine that need to be displayed
    // in a more obvious way than EventMessage
    // forwards to MobiusListener::mobiusAlert
    EventAlert,

    // this was how we asked the UI to refresh closer to a subcycle/cycle/loop
    // boundary being crossed rather than waiting for the next 1/10th refresh cycle
    // it made the UI appear more accurate for things like the beaters that were supposed
    // to pulse at regular intervals
    // since this happens frequently and is simple, it doesn't have to be a ThreadEvent
    // it could just be a KernelMessage type?
    // forwards to MobiusListener::mobiusTimeBoundary
    EventTimeBoundary,

    // should no longer be necessary
    EventUnitTestSetup,

    // sent by the script interpreter when a script finishes execution
    // used only by the TestDriver
    EventScriptFinished,

    // handler for the old script parameter "set bindings <name>"
    EventActivateBindings

} KernelEventType;

/**
 * Maximum length of the string that may be placed in a
 * KernelEvent argument array.
 *
 * This was 1024 which seems high, but we won't have many of these.
 */
const int KernelEventMaxArg = 1024;
    
/**
 * The main event.
 */
class KernelEvent
{
  public:

    KernelEvent();
    ~KernelEvent();
    void init();

    // unused events are maintained in a pool
    KernelEvent* next;

    // what the Kernel wants to do
    KernelEventType type;

    // continue what ThreadEvent did by having three arguments
    // whose contents depend on the type
    char arg1[KernelEventMaxArg];
    char arg2[KernelEventMaxArg];
    char arg3[KernelEventMaxArg];

    // the return code sent back down for the EventPrompt event
    // this was the only event that could return something
    int returnCode;

    // EventSaveProject used to pass entire Projects around
    // not sure I like this
    class Project* project;

    // set an argument with the usual bounds checking
    // returns true if the value fit, calls are encouraged to bail if it doesn't
    // this was used a lot to pass file paths but we really shouldn't be doing
    // long paths in scripts anyway and they should always be relative
    // to something the container gets to decide
    bool setArg(int number, const char* value);

    // kludge: want to use KernelEvent in TestDriver to communication when
    // a test script finishes.  The original UIAction.requesetId was saved on the
    // ScriptIntrepreter and now needs to be passed back up.  This is the first
    // non-string arg, could have more of these...
    int requestId;

  private:


};

/**
 * The usual linked list pool.
 * 
 * REALLY need to generalize this into a common base pool class
 * and stop duplicating this.  I tried that with ObjectPools in old code
 * but it never worked right, need to revisit that.
 *
 * EventPool is weird because of all the interconnections, but Action
 * could use this and probably others.  Maybe Layer.
 *
 * Flexible capacity maintenance is MUCH less important here than it
 * is for KernelMessage since kernel events are rare.  It would take
 * a rogue script vomiting Save requests to deplete it.
 *
 * Because Shell won't be checking capacity, we don't have to worry
 * about thread safety yet, only Kernel can touch this.
 */
class KernelEventPool
{
  public:

    KernelEventPool();
    ~KernelEventPool();

    KernelEvent* getEvent();
    void returnEvent(KernelEvent* e);

    void checkCapacity(int desired);
    void dump();
    
  private:

    KernelEvent* mPool;
    int mAllocated;
    int mUsed;

};


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
