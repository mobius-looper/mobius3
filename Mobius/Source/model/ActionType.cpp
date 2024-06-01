/**
 * UPDATE: This is no longer used by UIAction.  It has been replaced
 * by Symbol.  It is still used in some places of the core Action model
 * and should be gradually removed.
 *
 * old comments:
 * A collection of static object that define the types of actions
 * that can be taken on the system core from the user interface.
 * This is part of the Binding and Action models but factored out
 * so they can be used at various levels without needing to know
 * where they came from.
 * 
 * They self initialize during static initialization and will
 * self destruct.
 *
 * They are currently called Operations which I was playing with
 * for awhile but decided I don't like and will rename eventually.
 *
 */

#include "../util/Util.h"
#include "ActionType.h"

//////////////////////////////////////////////////////////////////////
//
// ActionTypes
//
//////////////////////////////////////////////////////////////////////

std::vector<ActionType*> ActionType::Instances;

ActionType::ActionType(const char* name, const char* display) :
    SystemConstant(name, display)
{
    Instances.push_back(this);
}

// can't we push this shit into SystemConstant and share it!?
ActionType* ActionType::find(const char* name) 
{
	ActionType* found = nullptr;

	if (name != nullptr) {
		for (int i = 0 ; i < Instances.size() ; i++) {
			ActionType* op = Instances[i];
			if (!strcmp(op->getName(), name)) {
				found = op;
				break;
			}
		}
	}
	return found;
}

ActionType ActionFunctionObj("function", "Function");
ActionType* ActionFunction = &ActionFunctionObj;

ActionType ActionParameterObj("parameter", "Parameter");
ActionType* ActionParameter = &ActionParameterObj;

ActionType ActionActivationObj("activation", "Activation");
ActionType* ActionActivation = &ActionActivationObj;

ActionType ActionScriptObj("script", "Script");
ActionType* ActionScript = &ActionScriptObj;

ActionType ActionSampleObj("sample", "Sample");
ActionType* ActionSample = &ActionSampleObj;

// should just be an enumeration or another SystemConstant

ActionType ActionPresetObj("preset", "Preset");
ActionType* ActionPreset = &ActionPresetObj;

ActionType ActionSetupObj("setup", "Setup");
ActionType* ActionSetup = &ActionSetupObj;

ActionType ActionBindingsObj("bindings", "Bindings");
ActionType* ActionBindings = &ActionBindingsObj;

//////////////////////////////////////////////////////////////////////
//
// ActionOperator
//
//////////////////////////////////////////////////////////////////////

std::vector<ActionOperator*> ActionOperator::Instances;

ActionOperator::ActionOperator(const char* name, const char* display) :
    SystemConstant(name, display)
{
    Instances.push_back(this);
}

/**
 * Find an Operator by name
 * This doesn't happen often so we can do a liner search.
 */
ActionOperator* ActionOperator::find(const char* name)
{
	ActionOperator* found = nullptr;
	
    // todo: need to support display names?
	for (int i = 0 ; i < Instances.size() ; i++) {
		ActionOperator* op = Instances[i];
		if (StringEqualNoCase(op->getName(), name)) {
            found = op;
            break;
        }
	}
	return found;
}

ActionOperator OperatorMinObj{"min", "Minimum"};
ActionOperator* OperatorMin = &OperatorMinObj;

ActionOperator OperatorMaxObj{"max", "Maximum"};
ActionOperator* OperatorMax = &OperatorMaxObj;

ActionOperator OperatorCenterObj{"center", "Center"};
ActionOperator* OperatorCenter = &OperatorCenterObj;

ActionOperator OperatorUpObj{"up", "Up"};
ActionOperator* OperatorUp = &OperatorUpObj;

ActionOperator OperatorDownObj{"down", "Down"};
ActionOperator* OperatorDown = &OperatorDownObj;

ActionOperator OperatorSetObj{"set", "Set"};
ActionOperator* OperatorSet = &OperatorSetObj;

// todo: this sounds dangerous, what did it do?
ActionOperator OperatorPermanentObj{"permanent", "Permanent"};
ActionOperator* OperatorPermanent = &OperatorPermanentObj;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
