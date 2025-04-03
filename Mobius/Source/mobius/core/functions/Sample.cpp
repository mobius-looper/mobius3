/**
 * Function necessary for scripts to trigger samples.
 * Only used internally by test scripts, in the new world
 * SampleTrigger is haneled by the kernel above the core.
 *
 * When scripts see a statement keyword that does not resolve to an
 * internal script object, it expects it to be the name of a Function.
 *
 * This special case is necessary because SamplePlay is now a Kernel level
 * UIAction and never makes it down to the core.  So normally the core doesn't
 * need to be aware of samples EXCEPT for scripts which expect to be able to
 * anything.
 *
 * This is kind of messy and if we start having more of these need to oome up
 * with a more general way for core to send actions UP to kernel rather than
 * the other way around.  For now, the Function object is there so the
 * script has something to resolve to, but we can't get here through a normal Action
 * or Event, it is only invoked from scripts.
 *
 * todo: Revisit this after the grand Symbol makeover and send a UIAction up
 *
 */

#include "../../MobiusKernel.h"

#include "../Function.h"
#include "../Action.h"
#include "../Mobius.h"

class SampleFunction : public Function {
  public:
	SampleFunction();
	void invoke(Action* action, Mobius* m);
};

SampleFunction CoreSamplePlayObj;
Function* CoreSamplePlay = &CoreSamplePlayObj;

SampleFunction::SampleFunction()
{
	global = true;
	//noFocusLock = true;

    setName("Sample");
    scriptOnly = true;
}

void SampleFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

        int sampleIndex = action->arg.getInt();

        // args are 1 based, convert
        sampleIndex--;

        if (sampleIndex >= 0) {
            MobiusKernel* k = m->getKernel();
            k->coreSampleTrigger(sampleIndex);
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
