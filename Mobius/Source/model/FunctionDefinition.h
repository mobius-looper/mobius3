/* 
 * Model for function definitions.
 *
 * Functions are commands that can be sent to the engine.
 * They differ from Parameters in that they do not have values
 * and cannot be configured.
 *
 * TODO: Check VST3 and see if plugins have a similar concept that
 * could be used these days.
 *
 * This corresponds to the core Function model which is very complex
 * and has more than should be exposed above the engine.  All we need for
 * the UI is a set of names to build bindings, and a few operational
 * properties like "sustainable" to determine how to process bindings.
 *
 * The UI model and core model are associated through a common Symbol.
 *
 * This is slowly evolving.  It still maintains a set of static constant
 * objects for each of the core functions we care about for bindings.
 * I'd like to move to having these all be dynamically configured from
 * an XML file.
 *
 */

#pragma once

#include <vector>

#include "SystemConstant.h"

//////////////////////////////////////////////////////////////////////
//
// FunctionDefinition
//
//////////////////////////////////////////////////////////////////////

class FunctionDefinition : public SystemConstant {

  public:
    
	FunctionDefinition(const char* name);
    virtual ~FunctionDefinition();


    // things copied from the core model into the outer one
    // temporary until loading definitions from the symbols.xml file is fleshed out
    
    /**
     * When true, this function may respond to a sustained action.
     */
    bool sustainable = false;

    /**
     * When true, this function may be focus locked.
     */
    bool mayFocus = false;

    /**
     * Can the function operate as a switch confirmation.
     */
    bool mayConfirm = false;

    /**
     * Can the function cancel mute mode.
     */
    bool mayCancelMute = false;

    //////////////////////////////////////////////////////////////////////
    // Global Function Registry
    //////////////////////////////////////////////////////////////////////

    static std::vector<FunctionDefinition*> Instances;
	static FunctionDefinition* find(const char* name);
    static void trace();

  private:

};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
