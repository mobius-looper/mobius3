/**
 * ConfigEditor for the MIDI tracks.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../Supervisor.h"

#include "../common/SimpleRadio.h"

#include "MidiTrackEditor.h"

MidiTrackEditor::MidiTrackEditor(Supervisor* s) : ConfigEditor(s), form(s)
{
    setName("MidiTrackEditor");
    render();
}

MidiTrackEditor::~MidiTrackEditor()
{
}

void MidiTrackEditor::prepare()
{
}

void MidiTrackEditor::resized()
{
    form.setBounds(getLocalBounds());
}

//////////////////////////////////////////////////////////////////////
//
// ConfigEditor overloads
//
//////////////////////////////////////////////////////////////////////

void MidiTrackEditor::load()
{
    Session* src = supervisor->getSession();
    session.reset(new Session(src));
    revertSession.reset(new Session(src));
    
    selectedTrack = 0;
    trackSelector.setSelection(selectedTrack);
    
    loadSession();
}

/**
 * Called by the Save button in the footer.
 *
 * Replace the Session::Tracks in the master Session with
 * ones in the edited Session.
 *
 * This only includes the MIDI tracks right now.
 */
void MidiTrackEditor::save()
{
    saveSession();

    Session* master = supervisor->getSession();
    
    master->replaceMidiTracks(session.get());

    supervisor->updateSession();

    session.reset(nullptr);
    revertSession.reset(nullptr);
}

/**
 * Throw away all editing state.
 */
void MidiTrackEditor::cancel()
{
    session.reset(nullptr);
    revertSession.reset(nullptr);
}

void MidiTrackEditor::revert()
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
void MidiTrackEditor::loadSession()
{
    trackCount.setInt(session->midiTracks);
    loadTrack(selectedTrack);
}

void MidiTrackEditor::loadTrack(int index)
{
    Session::Track* track = session->getTrack(Session::TypeMidi, index);
    if (track != nullptr) {
        form.load(track->getParameters());
    }
    else {
        // didn't have a definition for this one, reset the fields to initial values
        form.load(nullptr);
    }
}

void MidiTrackEditor::saveSession()
{
    session->midiTracks = trackCount.getInt();
    saveTrack(selectedTrack);
}

void MidiTrackEditor::saveTrack(int index)
{
    Session::Track* track = session->ensureTrack(Session::TypeMidi, index);
    ValueSet* params = track->ensureParameters();
    form.save(params);
}

//////////////////////////////////////////////////////////////////////
//
// Form Rendering
//
//////////////////////////////////////////////////////////////////////

void MidiTrackEditor::render()
{
    trackCount.setListener(this);
    form.add(&trackCount);
    form.addSpacer();
    
    trackSelector.setButtonCount(8);
    trackSelector.setListener(this);
    form.add(&trackSelector);

    form.addField(ParamSyncSource);
    form.addField(ParamTrackSyncUnit);
    form.addField(ParamSlaveSyncUnit);
    form.addField(ParamBeatsPerBar);
    
    addAndMakeVisible(form);
}

//////////////////////////////////////////////////////////////////////
//
// Listneners
//
//////////////////////////////////////////////////////////////////////

/**
 * Respond to the track selection radio
 */
void MidiTrackEditor::radioSelected(YanRadio* r, int index)
{
    (void)r;
    saveTrack(selectedTrack);
    selectedTrack = index;
    loadTrack(selectedTrack);
}

void MidiTrackEditor::comboSelected(class YanCombo* c, int index)
{
    (void)c;
    (void)index;
    //Trace(2, "MidiTrackEditor: Sync source selected %d", index);
}

void MidiTrackEditor::inputChanged(class YanInput* input)
{
    (void)input;
    //Trace(2, "MidiTrackEditor: Track count changed %d", input->getInt());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
