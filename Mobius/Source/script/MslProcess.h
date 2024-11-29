/**
 * An object representing a non-transient MSL session that is being managed
 * one of the two shell/kernel contexts.
 *
 * Sessions start out transient and have no result or process.
 * Once a session needs to suspend for any reason (transition, wait, sustain, repeat)
 * It will be given a Process which may then be monitored.
 */

#pragma once

#include "MslObjectPool.h"

class MslProcess : public MslPooledObject
{
  public:

    typedef enum {
        StateNone,
        StateRunning,
        StateWaiting,
        StateSuspended,
        StateTransitioning
    } State;

    MslProcess();
    ~MslProcess();
    void poolInit() override;

    // process list chain pointer
    MslProcess* next = nullptr;

    // unique id, for correlating the session
    int id = 0;

    // state this process is in
    State state = StateNone;

    // the context that owns it
    MslContextId context;

    // display name for this process, taken from the MslCompilation
    // or MslLinkage
    // typically a script or functio name
    static const int MaxName = 64;
    char name[MaxName];
    
    // trigger id that caused this process to start
    // for correlating sustain and repeat actions
    int triggerId = 0;

    void setName(const char* s);
    void copy(MslProcess* src);

  private:

    // handle to the running session
    class MslSession* session = nullptr;

    // handle to the result created for this session
    class MslResult* result = nullptr;

};    

class MslProcessPool : public MslObjectPool
{
  public:

    MslProcessPool();
    virtual ~MslProcessPool();

    class MslProcess* newProcess();

  protected:
    
    virtual MslPooledObject* alloc() override;
    
};
