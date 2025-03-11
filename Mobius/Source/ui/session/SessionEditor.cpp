/**
 * ConfigEditor for the Session.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../model/ParameterSets.h"
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
    
    globalEditor->initialize(s, this);
    parameterEditor->initialize(s, this);
    trackEditor->initialize(s, this);

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

    invalidateSession();
    revertSession.reset(nullptr);
}

void SessionEditor::saveSession(Session* dest)
{
    ValueSet* globals = dest->ensureGlobals();
    globalEditor->save(globals);
    parameterEditor->save(globals);
    trackEditor->save(dest);
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
 * This once was where occllusion lists were refreshed assuming
 * that leaving tab MIGHT have changed the overlays, but that
 * is handled by YanParameter field listsners now.
 * Keep this around in case it's useful, then delete.
 */
void SessionEditor::basicTabsChanged(int oldIndex, int newIndex)
{
    (void)newIndex;
    (void)oldIndex;
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

    // SessionTrackForms need this
    refreshLocalOcclusions();
    
    // NOTE: Because TrackEditor needs access to all of the
    // ValueSets for every Session::Track, it is allowed to retain
    // a pointer to the initial intermediate Session
    trackEditor->load(session.get());
}

/**
 * Here via form field listeners whenever an overlay selection
 * changes.  Refresh the occlusion lists and tell the tracks about it.
 */
void SessionEditor::overlayChanged()
{
    refreshLocalOcclusions();
    trackEditor->sessionOverlayChanged();
}

//////////////////////////////////////////////////////////////////////
//
// Occlusions
//
//////////////////////////////////////////////////////////////////////

/**
 * Here on the initial load before tracks have been initialized.
 */
void SessionEditor::refreshLocalOcclusions()
{
    ValueSet* globals = session->ensureGlobals();
    gatherOcclusions(sessionOcclusions, globals, ParamSessionOverlay);
    gatherOcclusions(defaultTrackOcclusions, globals, ParamTrackOverlay);
}

void SessionEditor::gatherOcclusions(SessionOcclusions& occlusions, ValueSet* values,
                                     SymbolId sid)
{
    occlusions.clear();
    
    ParameterSets* sets = supervisor->getParameterSets();
    SymbolTable* symbols = supervisor->getSymbols();
    Symbol* ovsym = symbols->getSymbol(sid);
    const char* ovname = values->getString(ovsym->name);
    if (ovname != nullptr) {
        if (sets == nullptr) {
            Trace(1, "SessionEditor: Unresolved overlay name %s", ovname);
        }
        else {
            juce::String jovname = juce::String(ovname);
            ValueSet* overlay = sets->find(jovname);
            if (overlay == nullptr) {
                Trace(1, "SessionEditor: Unresolved overlay name %s", ovname);
            }
            else {
                juce::StringArray keys;
                overlay->getKeys(keys);
                for (auto key : keys) {
                    MslValue* v = overlay->get(key);
                    occlusions.add(jovname, key, v);
                }
            }
        }
    }
}

/**
 * Called by each SessionTrackForms buried under SessionTrackEditor to see if
 * a symobl is in either in the default track overlay or the session overlay.
 *
 * The track's own occlusion list is passed.  If this is not empty use it,
 * if it is empty then use the default track overlay.
 */
SessionOcclusions::Occlusion* SessionEditor::getOcclusion(Symbol* s, SessionOcclusions& trackOcclusions)
{
    SessionOcclusions::Occlusion* occlusion = trackOcclusions.get(s->name);

    if (occlusion == nullptr)
      occlusion = defaultTrackOcclusions.get(s->name);
    
    if (occlusion == nullptr)
      occlusion = sessionOcclusions.get(s->name);
    
    return occlusion;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
