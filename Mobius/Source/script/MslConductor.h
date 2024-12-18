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
 * modification of them.
 *
 * The end result is that the MslSession becomes the single means of communication betwee
 * the shell and the kernel within the script environment.  It behaves somewhat like
 * KernelCommunicator does within the Mobius engine but differs in that the "message"
 * being passed is almost always MslSession and sessions can have an indefinite lifespan
 * while KernelMessage it normally consumed and reclaimed immediately.
 *
 * The MslMessage is used to shuttle sessions between contexts, and also used for a few
 * additional tasks like forwarding application requests, and storing session results.
 *
 * I wish I had a more general thread/memory-safe container class for things like this
 * since this pattern is happening in three places now.
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
    class MslResult* runInitializer(class MslContext* c, class MslCompilation* unit,
                                    class MslBinding* arguments, class MslNode* node);

    class MslResult* run(class MslContext* c, class MslCompilation* unit, class MslBinding* arguments);
    class MslResult* request(class MslContext* c, class MslRequest* req);
    
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
    class MslProcess* makeProcess(class MslSession* s);

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

    void doRequest(class MslContext* c, class MslMessage* msg);
    MslResult* resume(class MslContext* c, class MslRequest* req, class MslSession* session);
    MslResult* start(class MslContext* c, class MslRequest *req);
    MslResult* release(class MslContext* c, class MslRequest* req, class MslSession* session);
    MslResult* repeat(class MslContext* c, class MslRequest* req, class MslSession* session);
    class MslSession* findSuspended(class MslContext* c, int triggerId);
    MslContextId probeSuspended(class MslContext* c, int triggerId);
    void sendRequest(class MslContext* c, class MslRequest* req);

};



    
    
