/**
 * Encapsulates most of the core code related to scripts.
 *
 * There are two parts to this: compilation and runtime.
 * Would like to split compilation into something more self
 * contained that doesn't drag in runtime dependencies
 * and defer linkage to internal objects like Function
 * and Parameter.
 *
 */

#include "../../util/Trace.h"
#include "../../model/ScriptConfig.h"

#include "Script.h"
#include "ScriptCompiler.h"
#include "ScriptRuntime.h"
#include "Function.h"

#include "Scriptarian.h"
#include "Mem.h"

Scriptarian::Scriptarian(Mobius* argMobius)
{
    mMobius = argMobius;
    mLibrary = nullptr;
    mRuntime = NEW1(ScriptRuntime, mMobius);
}

Scriptarian::~Scriptarian()
{
    delete mLibrary;
    delete mRuntime;
}

/**
 * Compile the scripts referenced in a ScriptConfig, link
 * them to Function and Parameter objects, and build out the
 * combined Function array containing both static and script functions.
 *
 * This is used by the Shell to do all of the memory allocation and
 * syntax analysis outside the audio thread.  It will later be passed
 * down to the core for installation.
 *
 * For historical reasons, this needs a Mobius to operate for
 * reference resolution.  The compilation process must have NO side
 * effects on the core runtime state.  It is allowed to get the
 * MobiusConfig from Mobius, but this may not be where this
 * ScriptConfig came from.
 *
 */
void Scriptarian::compile(ScriptConfig* src)
{
    ScriptCompiler* sc = NEW(ScriptCompiler);

    // revisit the interface, rather than passing mMobius can
    // we pass ourselves intead?
    // it will want to look up Functions but also Parameters
    mLibrary = sc->compile(mMobius, src);
    delete sc;

    // !! need a way to pass the compiler error list back up
}    

//////////////////////////////////////////////////////////////////////
//
// Runtime Pass Throughs
//
// We've got three layers of this now, and I'm unconfortable
// Mobius is what most of the system calls and it passes to Scriptarian
// Scriptarian passes to ScriptRuntime
//
//////////////////////////////////////////////////////////////////////

void Scriptarian::doScriptMaintenance()
{
    mRuntime->doScriptMaintenance();
}

void Scriptarian::finishEvent(KernelEvent* e)
{
    mRuntime->finishEvent(e);
}

/**
 * RunScriptFunction global function handler.
 * RunScriptFunction::invoke calls back to to this.
 */
void Scriptarian::runScript(Action* action)
{
    // everything is now encapsulated in here
    mRuntime->runScript(action);
}

void Scriptarian::resumeScript(Track* t, Function* f)
{
    mRuntime->resumeScript(t, f);
}

void Scriptarian::cancelScripts(Action* action, Track* t)
{
    mRuntime->cancelScripts(action, t);
}

/**
 * Used by Mobius to phase in a new Scriptarian containing
 * a newly loaded Script model.  This can't be done if any
 * Scripts are still running.
 */
bool Scriptarian::isBusy()
{
    return mRuntime->isBusy();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
