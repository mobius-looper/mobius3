/**
 * Incredibly hacky, but just trying to get something functional and we'll
 * see about organization when the dust settles.
 */

#include <JuceHeader.h>

#include "MslScript.h"
#include "MslParser.h"
#include "MslSession.h"
#include "MslEnvironment.h"

#include "MslScriptletSession.h"


MslScriptletSession::MslScriptletSession(MslEnvironment* env)
{
    environment = env;

    script.reset(new MslScript());
}

MslScriptletSession::~MslScriptletSession()
{
    // todo: if we installed anything in the Environment,
    // make sure it is removed?
    MslValuePool* vp = environment->getValuePool();
    vp->free(scriptletResult);
}

void MslScriptletSession::reset()
{
    // todo: remove procs and vars installed in the environment?

    script.reset(new MslScript());
}

/**
 * Okay, now it gets interesting.
 * Parse the scriptlet text and evaluate it.
 */
bool MslScriptletSession::eval(MslContext* c, juce::String source)
{
    MslParser parser;

    errors.clear();

    // special parser interface where we manage the MslScript on the outside
    MslParserResult* presult = parser.parse(script.get(), source);

    if (presult->errors.size() > 0) {
        MslError::transfer(&(presult->errors), errors);
    }
    else {
        // handoff is awkward
        // if we think of ScriptSession as living outside environment then
        // we have to ask it to do a lot
        // if it lives inside, then we have to go through the environment
        // for everything
        MslSession* session = new MslSession(environment);

        session->start(c, script.get());

        // the now familiar copying of result/error status from one object
        // to another
        // might be worth factoring out an MslErrorContainer or something we can pass
        // down rather than always moving it back up
        MslError::transfer(session->getErrors(), errors);

        // temporary diagnostics
        fullResult = session->getFullResult();
        
        // scriptlets to not support complex values, though I suppose they could
        MslValuePool* vp = environment->getValuePool();
        vp->free(scriptletResult);
        scriptletResult = session->captureResult();

        if (!session->isWaiting()) {
            delete session;
        }
        else {
            waitingSession.reset(session);
        }
    }

    delete presult;
    return (errors.size() == 0);
}

bool MslScriptletSession::isWaiting()
{
    return (waitingSession != nullptr);
}

void MslScriptletSession::resume(MslContext* c, MslWait* w)
{
    if (waitingSession != nullptr) {
        waitingSession->resume(c, w);
        
        MslError::transfer(waitingSession->getErrors(), errors);
        fullResult = waitingSession->getFullResult();
        MslValuePool* vp = environment->getValuePool();
        vp->free(scriptletResult);
        scriptletResult = waitingSession->captureResult();

        if (!waitingSession->isWaiting())
          waitingSession = nullptr;
    }
    else {
        waitingSession = nullptr;
    }
}

juce::OwnedArray<MslProc>* MslScriptletSession::getProcs()
{
    return &(script->procs);
}

    
