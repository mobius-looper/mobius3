/**
 * An object holding both the incomplete runtime status of an MslSession and the
 * final result after it finishes.
 *
 * One of these is created whenever a script or scriptlet is launched.  If the script
 * runs to completion synchronously, it is returned to the caller.  If the
 * script becomes asynchronous it is placed on the environments result list where
 * it can be inspected in the script console.
 *
 * todo: disliking the name ambiguity between "result" being the complex result
 * from a running session and "result value" being the MslValue that was computed by
 * the script.  Here we use "value" but session uses "result" and "childResults".
 */

#pragma once

class MslResult
{
    friend class MslEnvironment;
    friend class MslConductor;
    friend class MslPools;
    
  public:

    MslResult();
    ~MslResult();
    void init();
    
    // the id of the asynchronous session that produced this result
    int sessionId = 0;

    // the final result value when the session finishes without errors
    class MslValue* value = nullptr;

    // the list of errors accumulated at runtime
    class MslError* errors = nullptr;

    // chain pointer accessor so the console may iterate over results
    MslResult* getNext();
    
    // true if this session is still running
    bool isRunning();

  protected:

    // the chain pointer for the environment's result list
    MslResult* next = nullptr;

    // becomes true once the result has been installed on the environment's global result list
    bool interned = false;
    
    // direct pointer to the session if it is still running
    class MslSession* session = nullptr;

  private:

};

