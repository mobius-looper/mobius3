/**
 * Code related to the processing of UIActions sent to Mobius.
 * This is mostly old code factored out of Mobius to reduce the size.
 *
 * Mapping between the new UIAction model and old Action is done here.
 *
 */

#pragma once

#include <JuceHeader.h>

class Actionator
{
  public:

    Actionator(class Mobius* m);
    ~Actionator();
    void dump();

    //////////////////////////////////////////////////////////////////////
    // New Model
    //////////////////////////////////////////////////////////////////////

    // do a queueud action at the start of each block, or from a script
    void doAction(UIAction* a);
    
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
    
    // new model
    void doFunction(UIAction* action, Function* f);
    void doScript(UIAction* action);

    // old model
    void doFunctionOld(UIAction* action, Function* f);
    class Action* convertAction(UIAction* src);
    void doFunction(class Action* a);
    void doFunction(class Action* action, class Function* f, class Track* t);
};

