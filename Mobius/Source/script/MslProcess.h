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
#include "MslConstants.h"

class MslProcess : public MslPooledObject
{
    friend class MslConductor;
    
  public:

    MslProcess();
    ~MslProcess();
    void poolInit() override;

    // process list chain pointer
    MslProcess* next = nullptr;

    // unique id, for correlating the session
    int sessionId = 0;

    // state this process is in
    MslSessionState state = MslStateNone;

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

  protected:

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
