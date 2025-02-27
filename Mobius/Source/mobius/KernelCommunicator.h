/**
 * An object that sits between the shell and the kernel
 * and passes information between them with thread safety.
 *
 * KernelMessage deifnes a common generic model for all types
 * of messages that pass between the shell and the kernel.
 * It requires a "type" code to specify what kind of message it is,
 * may have a few arguments whose meaning is dependent on the type,
 * and may have a void* to a complex object relevant for that type.
 * Would be nice to work out some more elegant polymorphism here.
 *
 * KernelCommunicator is a singleton object shared by the shell and kernel
 * and contains several lists of KernelMessages.  These are pooled for
 * reuse and prevent memory management within the kernel.  Everying
 * is guarded with CriticalSections.
 *
 */

#pragma once

#include <JuceHeader.h>

//////////////////////////////////////////////////////////////////////
//
// KernelMessage
//
//////////////////////////////////////////////////////////////////////

/**
 * The types of messages
 *
 * Configure and Samples message are sent by the shell to the kernel
 * to update configuration objects.
 *
 * Action messages are sent from shell to kernel to perform an action.
 *
 * Event messages are sent from kernel to shell to do something
 * that can't be done in the kernel like file access or user interaction.
 * 
 */
typedef enum {

    MsgNone = 0,
    MsgConfigure,
    MsgAction,
    MsgSamples,
    MsgScripts,
    MsgBinderator,
    MsgEvent,
    MsgLoadLoop,
    MsgMidi,
    MsgMidiLoad

} MessageType;

/**
 * A union of the various objects that can be passed in a message.
 * Don't really need this but it's sligly more visually appealing
 * than a blind cast.
 */
typedef union {

    void* pointer;
    class ConfigPayload* payload;
    class UIAction* action;
    class SampleManager* samples;
    class Scriptarian* scripts;
    class Binderator* binderator;
    class KernelEvent* event;
    class Audio* audio;
    class MidiEvent* midi;
    class MidiSequence* sequence;
    
} MessageObject;

/**
 * A message object that can be passed up or down.
 * Messages may be maintined on one of several linked lists.
 * avoiding vectors right now to reduce memory allocation headaches
 * and a good old fashioned list works well enough and is encapsulated.
 */
class KernelMessage
{
  public:

    // message list chain, nullptr if not on a list
    KernelMessage* next = nullptr;

    // what it is
    MessageType type = MsgNone;

    // what it has
    MessageObject object;

    // special for MidiMessage
    juce::MidiMessage midiMessage;
    int deviceId = 0;
    
    // for loadLoop, possibly others someday
    int track = 0;
    int loop = 0;
    
    // todo: a few fixed arguments so we don't have to pass objects
    
    void init();
};

/**
 * The singleton object used for communciation between the shell and the kernel.
 * Maintains the following message lists.
 *
 *    pool          a pool of free messages
 *    toKernel      a list of message sent from the shell to the kernel
 *    toShell       list of message sent from the kernel to the shell
 *
 * The kernel consumes it's event list at the start of every audio interrupt.
 * The shell consumes it's event list during performMaintenance which is normally
 * called by a timer thread with 1/10 a second interval.
 *
 * During consumption, the receiver will call either popShell() or popKernel()
 * to obtain the next message in the queue.  After processing it should return
 * this to the pool with free().
 *
 * During interval processing a message to be sent is allocated with alloc(),
 * filled out with content, then added to one of the lists with either pushShell()
 * or pushKernel()
 *
 * Only the shell is allowed to periocially call checkCapacity() which will
 * make sure that the internal message pool is large enough to handle future
 * message allocations.
 *
 * If alloc() is called and the pool is empty, it will return nullptr.
 * In normal use this is almost always an indication of a memory leak.
 * In theory, a period of extremely intense activity could need more messages
 * than we have available but that really shouldn't happen in practice.
 * Rogue scripts would be the only possible example.
 *
 * Statistics are maintained and may be traced for leak diagnostics.
 *
 * To help detect leaks, the following pattern must be followed.
 *
 * Shell
 *
 *    shellAlloc
 *      allocate a message from the pool and bump the shellUsing counter
 *      this must be followed by either shellAbandon or shellSend
 *      normally this would be at most 1, but under the right conditions
 *      the UI thread and our MainThread could be touching the shell at
 *      the same time, this should go away if we switch to using juce::Timer
 *
 *    shellAbandon
 *      shell decided not to use the message returned by shellAlloc
 *      can't think of a reason for this, but might have one
 *      decrements shellUsing
 *   
 *    shellSend
 *      shell sends the message from shellAlloc to the kernel
 *      decrements shellUsing
 *
 *    shellReceive
 *      shell retrieves a message sent by the kernel, increment shellUsing
 *      this must be followed by shellAbandon or pushKernel
 *      usually this would be shellAbandon because there are few if any
 *      cases where the shell wants to reuse a message it just popped to
 *      send back to the kernel
 *
 * Kernel
 *    same pattern going the other direction
 *
 *    kernelAlloc
 *      must be followed by kernelAbandon or kernelSend
 *
 *    kernelReceive
 *      must be followed by kernelAbandon or kernelSend
 *      kernel usually calls kernelSend rather than kernelAbandon
 *      because it reuses the message sent by the shell to send a response
 *    
 */
class KernelCommunicator
{
  public:

    KernelCommunicator();
    ~KernelCommunicator();

    // shell methods
    KernelMessage* shellAlloc();
    void shellAbandon(KernelMessage* msg);
    void shellSend(KernelMessage* msg);
    KernelMessage* shellReceive(bool ordered);  

    // kernel methods
    KernelMessage* kernelAlloc();
    void kernelAbandon(KernelMessage* msg);
    void kernelSend(KernelMessage* msg);
    KernelMessage* kernelReceive();  

    // only for shell maintenance
    void checkCapacity();
    void traceStatistics();
    
  private:

    KernelMessage* alloc();
    void free(KernelMessage* msg);

    void pushKernel(KernelMessage* msg);
    KernelMessage* popKernel();

    void pushShell(KernelMessage* msg);
    KernelMessage* popShell();
    
    juce::CriticalSection criticalSection;

    // the total number of message allocations created with new
    // normally also maxPool
    int totalCreated = 0;

    // shared free pool
    KernelMessage* pool = nullptr;
    int poolSize = 0;

    // shell message queue
    KernelMessage* toShell = nullptr;
    int shellSize = 0;
    int shellUsing = 0;
    
    // kernel queue
    KernelMessage* toKernel = nullptr;
    int kernelSize = 0;
    int kernelUsing = 0;
    
    int minPool = 0;
    int maxShell = 0;
    int maxKernel = 0;
    int poolExtensions = 0;
    int totalShellSends = 0;
    int totalKernelSends = 0;
    
    void deleteList(KernelMessage* list);
        
};

//
// Tuning constants for pool capacity
//

/**
 * The inital size of the pool.
 * This should ideally be set high enough to avoid additional
 * allocations during normal use.
 */
const int KernelPoolInitialSize = 20;

/**
 * The threshold for new allocations.
 * If the free pool dips below this size, another block
 * is allocated.
 */
const int KernelPoolSizeConcern = 5;

/**
 * The number of messages to allocate when the
 * SizeConern threshold is reached.
 */
const int KernelPoolReliefSize = 10;

/**
 * The number of messages on the shell or kernel queue above which
 * we start to question our life choices.
 */
const int KernelPoolUseConcern = 3;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
