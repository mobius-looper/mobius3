/**
 * Primary Mobius sub component for managing scripts.
 */

#pragma once

class Scriptarian
{
  public:

    Scriptarian(class Mobius* m);
    ~Scriptarian();

    // this is where compilation happens
    void compile(class ScriptConfig* src);

    // library access just for Shell to build the DynamicConfig
    // update: not used after SymbolTable
    class MScriptLibrary* getLibrary() {
        return mLibrary;
    }

    // Runtime control
    
    void runScript(class Action* action);
    void resumeScript(class Track* t, class Function* f);
    void cancelScripts(class Action* action, class Track* t);
    bool isBusy();

    // advance the runtime on each audio interrupt
    void doScriptMaintenance();

    // notifications about a previously scheduled event finishing
    void finishEvent(class KernelEvent* e);
    
  private:

    class Mobius* mMobius;

    // compilation artifacts
    class MScriptLibrary* mLibrary;

    // consider whether this needs to be distinct
    // or can we just merge it with Scriptarian
    class ScriptRuntime* mRuntime;

};
