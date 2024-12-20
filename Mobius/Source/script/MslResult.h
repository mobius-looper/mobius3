/**
 * An object holding results from one execution of a script.
 *
 * This may contain one or more values if the script performed a computation
 * and may contain one or more error messages if the script encountered errors.
 * If the script could not be completed synchronously it will contain a "session id"
 * which can be used to find the results of the script later when it finishes.
 *
 * Results may either be "free" or "saved".  Free results are owned by the application
 * and muse be deleted.  Saved results are owned by the Environment and are deleted
 * by the Environment under controlled conditions.  Free results may be added to the
 * saved result list if desired.
 *
 * If a script request runs to completion synchronously a free result is returned.
 * If the script needs to suspend, an saved result is created which may be inspected
 * later by the monitoring UI.
 *
 */

#pragma once

#include "MslConstants.h"

class MslResult
{
    friend class MslEnvironment;
    friend class MslConductor;
    friend class MslPools;
    
  public:

    MslResult();
    ~MslResult();
    void init();
    
    // the id of the asynchronous session that saved this result
    // or the id of the session that is running in the background to produce a result
    int sessionId = 0;

    // session state, normally Finished, Waiting, or Suspended
    // and for very brief moments Transitioning
    // todo: this shouldn't be necessary any more how that we have MslProcess?
    MslSessionState state = MslStateNone;

    // the final result value when the session finishes without errors
    class MslValue* value = nullptr;

    // the list of errors accumulated at runtime
    class MslError* errors = nullptr;

    // arbitrary results that can be added by the script and force persistence
    class MslValue* results = nullptr;
    
    // for saved results, the chain pointer for the list of saved results
    // used by the monitoring UI
    MslResult* getNext();

    // logging name
    // what's this for?
    static const int MslResultMaxName = 64;
    char name[MslResultMaxName];
    void setName(const char* s);
    
  protected:

    // the chain pointer for the environment's result list
    MslResult* next = nullptr;

    // becomes true once the result has been installed on the environment's global result list
    // internment is no longer a concept, they aren't put on the result list without
    // the session being finished
    //bool interned = false;

  private:

};

/**
 * Class used internally to assist building complex results.
 * May also be used by the application to assemble results to be returned
 * by mslAction and mslQuery.
 */
class MslResultBuilder
{
  public:

    MslResultBuilder();
    MslResultBuilder(class MslEnvironment* env);
    MslResultBuilder(MslResult* res);
    MslResultBuilder(class MslEnvironment* env, MslResult* res);
    ~MslResultBuilder();
    
    void addValue(int i);
    void addValue(const char* s);
    void addError(const char* s);

    MslResult* finish();
    
  private:

    class MslEnvironment* environment = nullptr;
    MslResult* result = nullptr;
    bool externalResult = false;
    
    void ensureResult();
    class MslError* allocError();
    
};
