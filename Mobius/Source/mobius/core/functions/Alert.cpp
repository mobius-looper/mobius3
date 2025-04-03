/**
 * Function necessary for scripts to send Alerts.
 *
 * Once scripts are able to resolve to Symbols, we won't need
 * a core Function object.  It would just find a Symbol
 * with LevelShell or LevelUI and send a UIAction upward.
 */

#include "../../../util/Util.h"
#include "../../MobiusKernel.h"

#include "../Function.h"
#include "../Action.h"
#include "../Mobius.h"

class AlertFunction : public Function {
  public:
	AlertFunction();
	void invoke(Action* action, Mobius* m);
};

AlertFunction AlertObj;
Function* Alert = &AlertObj;

AlertFunction::AlertFunction()
{
    setName("Alert");
	global = true;
    scriptOnly = true;
}

void AlertFunction::invoke(Action* action, Mobius* m)
{
	if (action->down) {
		trace(action, m);

        const char* msg = action->arg.getString();
        // todo: should this be an alert or a simple message?
        m->sendMobiusAlert(msg);
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
