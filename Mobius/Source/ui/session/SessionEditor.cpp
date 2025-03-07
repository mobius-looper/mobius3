/**
 * ConfigEditor for the Session.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../model/DeviceConfig.h"
#include "../../Supervisor.h"
#include "../../Provider.h"

#include "../common/BasicTabs.h"

#include "SessionGlobalEditor.h"
#include "SessionParameterEditor.h"
#include "SessionTrackEditor.h"

// temporary
#include "SessionTrackTable.h"

#include "SessionEditor.h"

SessionEditor::SessionEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("SessionEditor");

    trackEditor.reset(new SessionTrackEditor());
    tabs.add("Tracks", trackEditor.get());
    
    parameterEditor.reset(new SessionParameterEditor());
    tabs.add("Default Parameters", parameterEditor.get());

    globalEditor.reset(new SessionGlobalEditor());
    tabs.add("Globals", globalEditor.get());
    
    globalEditor->initialize(s);
    parameterEditor->initialize(s);
    trackEditor->initialize(s);

    addAndMakeVisible(tabs);
    tabs.setListener(this);
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
    invalidateSession();
    
    Session* src = supervisor->getSession();
    session.reset(new Session(src));
    revertSession.reset(new Session(src));

    // these are in the DeviceConfig but since there is no UI for that
    // show them as if they were session globals
    // hacky, needs thought
    DeviceConfig* dc = supervisor->getDeviceConfig();
    // the +1 is because the value is actually the number of "aux" pins and
    // there is always 1 "main" pin, but to the user it looks like they're combined
    session->setInt("pluginInputs", dc->pluginConfig.defaultAuxInputs + 1);
    session->setInt("pluginOutputs", dc->pluginConfig.defaultAuxOutputs + 1);
    
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

    // reverse the silly plugin pins thing
    // note that we have to get this from the master since that's
    // where we just committed the form changes
    ValueSet* globals = master->ensureGlobals();
    int newPluginInputs = getPortValue(globals, "pluginInputs", 8) - 1;
    int newPluginOutputs = getPortValue(globals, "pluginOutputs", 8) - 1;
    globals->remove("pluginInputs");
    globals->remove("pluginOutputs");
    
    // note that we don't call udateSession which will eventually go away
    // entirely, this will do track number normalization
    supervisor->sessionEditorSave();

    // convert the plugin pins back to the DeviceConfig
    DeviceConfig* dc = supervisor->getDeviceConfig();
    dc->pluginConfig.defaultAuxInputs = newPluginInputs;
    dc->pluginConfig.defaultAuxOutputs = newPluginOutputs;
    supervisor->updateDeviceConfig();

    invalidateSession();
    revertSession.reset(nullptr);
}

int SessionEditor::getPortValue(ValueSet* set, const char* name, int max)
{
    int value = set->getInt(name);
    if (value < 1)
      value = 1;
    else if (max > 0 && value > max)
      value = max;
    return value;
}

/**
 * Throw away all editing state.
 */
void SessionEditor::cancel()
{
    invalidateSession();
    revertSession.reset(nullptr);
}

void SessionEditor::decacheForms()
{
    invalidateSession();
    globalEditor->decacheForms();
    parameterEditor->decacheForms();
    trackEditor->decacheForms();
}

void SessionEditor::revert()
{
    invalidateSession();
    
    session.reset(new Session(revertSession.get()));
    
    loadSession();
}

/**
 * This must be used when the Session copied at load() needs to be
 * deleted, either when the form has been saved, reverted, or canceled.
 *
 * Since interior components are allowed to hold onto references to ValueSets
 * within this Session, they have to be informed and remove any references.
 * After this a load() traversal must be performed again.  Hit this after adding
 * decacheForms which tries to do a save if the form had been displayed, but at that
 * point, the editing session isn't always active and it got invalid memory access.
 */
void SessionEditor::invalidateSession()
{
    // ugly, when nwe delete the copied Session, need to inform the inner components
    // that any ValueSet prevously loaded must be forgotten
    globalEditor->cancel();
    parameterEditor->cancel();
    trackEditor->cancel();

    session.reset(nullptr);
}

/**
 * Called by BasicTabs whenever tabs change.
 */
void SessionEditor::basicTabsChanged(int oldIndex, int newIndex)
{
    (void)newIndex;
    //Trace(2, "SessionEditor: Tabs changed from %d to %d", oldIndex, newIndex);
    if (oldIndex == 2) {
        // formerly on the globals tab, on the off chance they changed
        // the session overlay refresh the track forms
        globalEditor->save(session->ensureGlobals());
        trackEditor->reload();
    }
    else if (oldIndex == 1) {
        // formerly on the defaults tab, also save to pick up parameter
        // changes and refresh the track forms
        parameterEditor->save(session->ensureGlobals());
        trackEditor->reload();
    }
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
    parameterEditor->load(globals);

    // NOTE: Because TrackEditor needs access to all of the
    // ValueSets for every Session::Track, it is allowed to retain
    // a pointer to the initial intermediate Session
    
    trackEditor->load(session.get());
}

/**
 * Deposit field results in this session, NOT the getEditingSession result.
 */
void SessionEditor::saveSession(Session* dest)
{
    ValueSet* globals = dest->ensureGlobals();
    globalEditor->save(globals);
    parameterEditor->save(globals);
    trackEditor->save(dest);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
