/**
 * The purpose of the Conductor is to manage the session lists used by the
 * shell and the kernel.  The code here is extremely sensitive so be careful.
 *
 * The session represents one execution of a script and it may be "running"
 * in either the shell or the kernel.  Kernel means it runs during the audio block
 * processing thread and shell means it runs in either the UI or maintenance threads.
 *
 * The UI and maintenance threads block each other with Juce's MessageLock so we don't
 * to worry about contention there, but the shell/kernel threads can be running at the same
 * time, particularly the kernel thread since audio block processing happens constantly
 * and rapidly.
 *
 * The audio block processing thread is not allowed to allocate or deallocate memory which
 * is why the usual smart container classes like juce::OwnedArray are not used.  Instead
 * lists are represented with old-school linked lists with critical sections guarding any
 * modofication of them.
 *
 * The end result is that the MslSession becomes the single means of communication betwee
 * the shell and the kernel within the script environment.  It behaves somewhat like
 * KernelCommunicator does within the Mobius engine but differs in that the "message"
 * being passes is only ever MslSession and sessions can have an indefinite lifespan
 * while KernelMessage it normally consumed and reclaimed immediately.
 *
 * I wish I had a more general thread/memory-safe container class for things like this
 * since this pattern is happening in three places now.
 *
 * There are two lists of sessions to be processed: shellSessions and kernelSessions.
 * While each side is being processed, if a session needs to transition to the other
 * side it is placed on the toShell or toKernel list.
 *
 * Use of an intermediate TO list allows each side to be iterating over the primary
 * list without a csect around the entire iteration.  Each side is not allowed to modify
 * the primary list of the other side, it can only add things to the TO list.
 * At the beginning of the processing cycle for each side, the TO list is moved onto
 * the primary list, guarded by csects just for the move.
 *
 * When a session completes, it may contain errors or other interesting information that
 * needs to be conveyed to the user.  These are accumulated on the result list by the shell.
 * If a session ends in the kernel, it is marked as completed and placed on the toShell list.
 * The shell then later consumes it and moves it to the result list.
 *
 * The result list is eventually consumed by the ScriptConsole.
 *
 */

#pragma once

#include <JuceHeader.h>

#include "MslConstants.h"
#include "MslMessage.h"
#include "MslProcess.h"

class MslConductor
{
  public:

    MslConductor(class MslEnvironment* env);
    ~MslConductor();

    // maintenance thread intterface
    void advance(class MslContext* c);
    
    // shell interface
    void pruneResults();
    class MslResult* getResults();

    // Environment interface
    class MslResult* start(class MslContext* c, class MslRequest *req, class MslLinkage* link);

    // resume after a wait
    void resume(class MslContext* c, class MslWait* wait);

    // results

    void saveResult(class MslContext* c, class MslResult* r);
    class MslResult* getResult(int id);
    
    bool captureProcess(int sessionId, class MslProcess& result);
    void listProcesses(juce::Array<MslProcess>& result);

    void enableResultDiagnostics(bool b);
    
  private:

    juce::CriticalSection criticalSection;
    juce::CriticalSection criticalSectionResult;

    class MslEnvironment* environment = nullptr;

    class MslSession* shellSessions = nullptr;
    class MslSession* kernelSessions = nullptr;
    class MslMessage* shellMessages = nullptr;
    class MslMessage* kernelMessages = nullptr;
    class MslResult* results = nullptr;
    class MslProcess* processes = nullptr;
    
    MslMessagePool messagePool;
    MslProcessPool processPool;
    int sessionIds = 1;
    bool resultDiagnostics = false;
    
    void deleteSessionList(class MslSession* list);
    void deleteResultList(class MslResult* list);
    void deleteMessageList(class MslMessage* list);
    void deleteProcessList(class MslProcess* list);

    int generateSessionId();
    void consumeMessages(class MslContext* c);
    void sendMessage(class MslContext* c, class MslMessage* msg);
    void doTransition(class MslContext* c, class MslMessage* msg);
    void addSession(class MslContext* c, class MslSession* s);
    void updateProcessState(class MslSession* s);
    void sendTransition(class MslContext* c, class MslSession* s);
    void addTransitioning(class MslContext* c, class MslSession* s);
    void addWaiting(class MslContext* c, class MslSession* s);
    void addProcess(class MslContext* c, class MslProcess* p);

    void advanceActive(class MslContext* c);
    class MslResult* checkCompletion(class MslContext* c, class MslSession* s);
    class MslResult* makeAsyncResult(class MslSession* s, MslSessionState state);
    class MslResult* finalize(class MslContext* c, class MslSession* s);
    bool removeProcess(class MslContext* c, class MslProcess* p);
    bool removeSession(class MslContext*c, class MslSession* s);
    class MslResult* makeResult(class MslContext* c, class MslSession* s);
    void doResult(class MslContext* c, class MslMessage* msg);
    
    void ageSuspended(class MslContext* c);
    void ageSustain(class MslContext* c, class MslSession* s, class MslSuspendState* state);
    void ageRepeat(class MslContext* c, class MslSession* s, class MslSuspendState* state);

    class MslResult* release(class MslContext* c, class MslRequest* req);
    class MslResult* repeat(class MslContext* c, class MslRequest* req, MslContextId otherContext);
    class MslSession* findSuspended(class MslContext* c, int triggerId);
    MslContextId probeSuspended(class MslContext* c, int triggerId);
    void sendNotification(class MslContext* c, MslNotificationFunction type, class MslRequest* req);
    void doNotification(class MslContext* c, class MslMessage* msg);
    void doRelease(class MslContext* c, class MslMessage* msg);
    void doRepeat(class MslContext* c, class MslMessage* msg);

};



    
    
