/**
 * ConfigEditor for the Session.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/SessionConstants.h"
#include "../../model/Session.h"
#include "../../model/MobiusConfig.h"
#include "../../Supervisor.h"

#include "../common/BasicTabs.h"
#include "../common/YanForm.h"
#include "../common/YanField.h"
#include "../common/SimpleRadio.h"

#include "../../sync/Transport.h"

#include "SessionEditor.h"

SessionEditor::SessionEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("SessionEditor");
    render();
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
    supervisor->updateSession();

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
 * Load a setup into the parameter fields
 */
void SessionEditor::loadSession()
{
    midiOut.setValue(session->getBool(SessionTransportMidiEnable));
    midiClocks.setValue(session->getBool(SessionTransportClocksWhenStopped));
}

void SessionEditor::saveSession(Session* dest)
{
    dest->setBool(SessionTransportMidiEnable, midiOut.getValue());
    dest->setBool(SessionTransportClocksWhenStopped, midiClocks.getValue());
}

//////////////////////////////////////////////////////////////////////
//
// Form Rendering
//
//////////////////////////////////////////////////////////////////////

void SessionEditor::render()
{
    transportForm.addSpacer();
    transportForm.add(&midiOut);
    transportForm.add(&midiClocks);
    
    tabs.add("Transport", &transportForm);

    addAndMakeVisible(tabs);
}

//////////////////////////////////////////////////////////////////////
//
// Listneners
//
//////////////////////////////////////////////////////////////////////

/**
 * Respond to the track selection radio
 */
void SessionEditor::radioSelected(YanRadio* r, int index)
{
    (void)r;
    (void)index;
}

void SessionEditor::comboSelected(class YanCombo* c, int index)
{
    (void)c;
    (void)index;
    //Trace(2, "SessionEditor: Sync source selected %d", index);
}

void SessionEditor::inputChanged(class YanInput* input)
{
    (void)input;
    //Trace(2, "SessionEditor: Track count changed %d", input->getInt());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
