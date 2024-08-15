/**
 * Code related to the processing of UIActions sent to Mobius.
 * This is mostly old code factored out of Mobius to reduce the size.
 *
 * Mapping between the new UIAction model and old Action is done here.
 *
 */

#pragma once

#include <JuceHeader.h>
#include "../../model/Scope.h"

class Actionator
{
  public:

    Actionator(class Mobius* m);
    ~Actionator();
    void dump();

    void refreshScopeCache(class MobiusConfig* config);
    
    //////////////////////////////////////////////////////////////////////
    // New Model
    //////////////////////////////////////////////////////////////////////

    // perform actions queued for the next interrupt
    // frames passed to advance the long-press trigger tracker
    void doInterruptActions(class UIAction* actions, long frames);

    // Parameter value access is in here too since
    // it has to do similar UI/core mapping and is small
    bool doQuery(class Query* q);

    // special interface for scripts that want immediate action
    // handling that isn't queued by doInterruptActions
    void doScriptAction(class UIAction* action);

    //////////////////////////////////////////////////////////////////////
    // Old Model
    //////////////////////////////////////////////////////////////////////

    // do an internally generated action
    void doOldAction(class Action* a);

    // this used to be in Mobius but it was moved down
    // here with the rest of the action code, where should this live?
    Track* resolveTrack(class Action* action);

    // action object management
    class Action* newAction();
    void freeAction(class Action* a);
    class Action* cloneAction(class Action* src);
    void completeAction(class Action* a);


  private:

    class Mobius* mMobius;
    class ActionPool* mActionPool;
    class TriggerState* mTriggerState;
    
    // new model
    void doCoreAction(UIAction* action);
    void doFunction(UIAction* action, Function* f);
    void doParameter(UIAction* action, Parameter* p);
    void doScript(UIAction* action);
    void doActivation(UIAction* action);

    // transitional model
    void doFunctionOld(UIAction* action, Function* f);
    void doFunctionNew(UIAction* action, Function* f);
    void doFunctionTracks(UIAction* action, Function* f);
    void doFunctionTrack(UIAction* action, Function* f, Track* t, bool needsClone);
    void doGroupReplication(Action* action, Function* f);
    class Action* convertAction(UIAction* src);
    void invoke(UIAction* action, Function* f);
    void invoke(UIAction* action, Function* f, Track* t);
    
    // old model
    void doFunction(class Action* a);
    void doFunction(class Action* action, class Function* f, class Track* t);
    void doParameter(class Action* a);
    void doParameter(class Action* a, class Parameter* p, class Track* t);

    void doPreset(class Action* a);
    void doSetup(class Action* a);

    int getParameter(Parameter* p, int trackNumber);

    // scope parsing
    ScopeCache scopes;

};

