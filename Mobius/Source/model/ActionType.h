/**
 * UPDATE: This is no longer used by UIAction.  It has been replaced
 * by Symbol.  It is still used in some places of the core Action model
 * and should be gradually removed.
 * 
 * old comments:
 * 
 * A collection of static object that define the types of actions
 * that can be taken on the system core from the user interface.
 * This is part of the Binding and Action models but factored out
 * so they can be used at various levels without needing to know
 * where they came from.
 * 
 * They self initialize during static initialization and will
 * self destruct.
 *
 */

#pragma once

#include <vector>

#include "SystemConstant.h"

//////////////////////////////////////////////////////////////////////
//
// Types
//
//////////////////////////////////////////////////////////////////////

/**
 * Defines the type of action, or which object within
 * the system will carry out that action.
 */
class ActionType : public SystemConstant {
  public:

    static std::vector<ActionType*> Instances;
	static ActionType* find(const char* name);

	ActionType(const char* name, const char* display);
};

extern ActionType* ActionFunction;
extern ActionType* ActionParameter;
extern ActionType* ActionActivation;

/**
 * This was a weird one.
 * Currently using this to indiciate calling a script which
 * will not have a resolved FunctionDefinition pointer at this level.
 * The UIAction will only have the name of the script to call and
 * possibly an ordinal.
 *
 * In old code this was, used to send down notification
 * of the completion of a ThreadEvent (now KernelEvent)
 * we don't do this using Actions any more.
 */
extern ActionType* ActionScript;

/**
 * Similar pseudo-action for actions representing sample playback.
 * This isn't used in a binding, but DynamicConfig now uses it to
 * pass information from the engine back to the UI about samples
 * that have been loaded.
 */
extern ActionType* ActionSample;

// until we can refactor all the old uses of TargetPreset
// and decide on the right concrete model, define these
// here just so we have a place to store the names
// they aren't really ActionTypes

extern ActionType* ActionSetup;
extern ActionType* ActionPreset;
extern ActionType* ActionBindings;

//////////////////////////////////////////////////////////////////////
//
// Operators
//
//////////////////////////////////////////////////////////////////////

/**
 * Constants that describe operations that produce a relative change to
 * a control or parameter.
 */
class ActionOperator : public SystemConstant {
  public:
    
    static std::vector<ActionOperator*> Instances;
    static ActionOperator* find(const char* name);

    ActionOperator(const char* name, const char* display);
};

extern ActionOperator* OperatorMin;
extern ActionOperator* OperatorMax;
extern ActionOperator* OperatorCenter;
extern ActionOperator* OperatorUp;
extern ActionOperator* OperatorDown;
extern ActionOperator* OperatorSet;
extern ActionOperator* OperatorPermanent;

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
