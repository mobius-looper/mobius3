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
 */

#pragma once

class MslConductor
{
  public:

    MslConductor(class MslEnvironment* env);
    ~MslConductor();

    // shell interface

    void shellMaintenance();
    class MslSession* getShellSessions();
    void addShellSession(class MslSession* s);
    void sendToKernel(class MslSession* s);

    // kernel interface

    void kernelMaintenance();
    class MslSession* getKernelSessions();
    void addKernelSEssion(class MslSEssion* s);
    void sendToShell(class MslSession* s);

  private:

    class MslEnvironment* environment = nullptr;

    MslSession* shellSessions = nullptr;
    MslSession* kernelSessions = nullptr;
    MslSession* toShell = nullptr;
    MslSession* toKernel = nullptr;

};



    
    
