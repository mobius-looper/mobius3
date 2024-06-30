/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * Global script execution state.  Encapsulates management of the 
 * script threads.
 * 
 * Factored out of Mobius.cpp because it was getting too big.
 *
 */

#ifndef SCRIPT_RUNTIME_H
#define SCRIPT_RUNTIME_H

class ScriptRuntime {
  public:

    ScriptRuntime(class Mobius* m);
    ~ScriptRuntime();

    void runScript(class Action* action);
    void doScriptMaintenance();

    // Functions call this to resume script waiting on a Function
    void resumeScript(class Track* t, class Function* f);

    // Resume scripts waiting on a KernelEvent
    void finishEvent(class KernelEvent* e);

    // Track calls this when it is Reset
    void cancelScripts(class Action* action, class Track* t);

    bool isBusy();

  private:

    // need to promote this
    void doScriptNotification(class Action* a);

    void startScript(class Action* action, class Script* script);
    void startScript(class Action* action, class Script* s, class Track* t);
    void addScript(class ScriptInterpreter* si);
    bool isInUse(class Script* s);
    class ScriptInterpreter* findScript(class Action* action, class Script* s, class Track* t);
    void freeScripts();


    class Mobius* mMobius;
	class ScriptInterpreter* mScripts;

    // number of script threads launched
    int mScriptThreadCounter;

};




#endif
