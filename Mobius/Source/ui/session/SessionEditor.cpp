/**
 * ConfigEditor for the Session.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../Supervisor.h"
#include "../../Provider.h"

#include "../common/BasicTabs.h"

#include "SessionGlobalEditor.h"
#include "SessionTrackEditor.h"

// temporary
#include "SessionTrackTable.h"

#include "SessionEditor.h"

SessionEditor::SessionEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("SessionEditor");

    globalEditor.reset(new SessionGlobalEditor());
    tabs.add("Parameters", globalEditor.get());

    trackEditor.reset(new SessionTrackEditor());
    tabs.add("Tracks", trackEditor.get());

    trackTable.reset(new SessionTrackTable());
    tabs.add("Table", trackTable.get());

    globalEditor->initialize(s);
    trackEditor->initialize(s);

    trackTable->initialize(s);

    addAndMakeVisible(tabs);
}

SessionEditor::~SessionEditor()
{
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
 */
void SessionEditor::loadSession()
{
    ValueSet* globals = session->ensureGlobals();
    
    globalEditor->load(globals);

    // NOTE: Because TrackEditor needs access to all of the
    // ValueSets for every Session::Track, it is allowed to retain
    // a pointer to the initial intermediate Session
    
    trackEditor->load(session.get());

    trackTable->load(supervisor, session.get());
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
