/**
 * ConfigEditor for the Session.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/SessionConstants.h"
#include "../../model/Session.h"
#include "../../model/SystemConfig.h"
#include "../../model/MobiusConfig.h"
#include "../../model/UIConfig.h"
#include "../../Supervisor.h"
#include "../../Provider.h"
#include "../JuceUtil.h"

#include "../common/BasicTabs.h"

#include "ParameterForm.h"
#include "SessionGlobalEditor.h"
#include "SessionTrackEditor.h"

#include "SessionEditor.h"

SessionEditor::SessionEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("SessionEditor");

    globalEditor.reset(new SessionGlobalEditor());
    tabs.add("Parameters", globalEditor.get());

    trackEditor.reset(new SessionTrackEditor());
    tabs.add("Tracks", trackEditor.get());

    globalEditor->initialize(s);
    trackEditor->initialize(s);

    addAndMakeVisible(tabs);
}

SessionEditor::~SessionEditor()
{
}

Provider* SessionEditor::getProvider()
{
    // !! This should be done by ConfigEditor and everything in this
    // ecosystem should stop using Supervisor directly
    return supervisor;
}

void SessionEditor::prepare()
{
}

void SessionEditor::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    tabs.setBounds(area);
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
//
//////////////////////////////////////////////////////////////////////

void SessionEditor::load()
{
    Session* src = supervisor->getSession();
    session.reset(new Session(src));
    revertSession.reset(new Session(src));
    
    loadSession();
}

/**
 * Called by the Save button in the footer.
 */
void SessionEditor::save()
{
    // Session editing state is currently all held in the Fields rather than the
    // copied Session so we can just update the master and abandon the copy
    Session* master = supervisor->getSession();
    saveSession(master);
    
    // note that we don't call udateSession which will eventually go away
    // entirely, this will do track number normalization
    supervisor->sessionEditorSave();

    // get rid of our intermediate state
    session.reset(nullptr);
    revertSession.reset(nullptr);
}

/**
 * Throw away all editing state.
 */
void SessionEditor::cancel()
{
    session.reset(nullptr);
    revertSession.reset(nullptr);
}

void SessionEditor::revert()
{
    session.reset(new Session(revertSession.get()));
    
    loadSession();
}

//////////////////////////////////////////////////////////////////////
//
// Internal Methods
// 
//////////////////////////////////////////////////////////////////////

/**
 * Load the internal forms from the session now being edited.
 * Things must call back to getEditingSession.
 */
void SessionEditor::loadSession()
{
    ValueSet* globals = session->ensureGlobals();
    
    globalEditor->load(globals);

    // !! yes, here's the issue
    // there isn't one ValueSet for the tracks there are N of them
    // trackEditor needs the entire Session

    trackEditor->load(session.get());
}

/**
 * Deposit field results in this session, NOT the getEditingSession result.
 */
void SessionEditor::saveSession(Session* dest)
{
    globalEditor->save(dest->ensureGlobals());
    trackEditor->save(dest);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
