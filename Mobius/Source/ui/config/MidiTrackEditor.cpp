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
    
    loadSession();

    // if we've had the panel open before, it will keep the radio
    // from the last time, have to put it back to match selectedTrack
    adjustTrackSelector();
}

/**
 * [notes copied from SetupEditor]
 *
 * Adjust the track selector Radio prior to loading to reflect
 * changes to the configured track count.  During static initialization
 * this was built out for 8 tracks.
 * ugh, this doesn't work, the radio just keeps going into the void and doesn't
 * size down, and the labels don't gain a 10th digit
 * probably an issue with the old SimpleRadio, but rather than trying to fix that
 * just switch over to a combo box if the size is more than 8.
 *
 * This transition only happens once, then it's combo all the way.
 * This is really ugly, but it gets the one user that needs more than 8 tracks
 * something usable.
 */
void MidiTrackEditor::adjustTrackSelector()
{
#if 0
    // setups only apply to core tracks so it is permissible to use this
    // rather than view->totalTracks
    int ntracks = session->getMidiTrackCount();
    if (ntracks > 8 && ntracks <= 32 && trackCombo == nullptr) {

        // it has been a radio
        // gak, Form/FormPanel is a mess, it requires a dynamic object that it
        // takes ownership of
        // SimpleRadio had it's own label, but since we're not in control of
        // layout, it's easier just to put the "label" in the item names
        trackCombo =  new juce::ComboBox();
        for (int i = 1 ; i <= ntracks ; i++) {
            trackCombo->addItem("Track " + juce::String(i), i);
        }
        trackCombo->addListener(this);

        // shit, Form/Field was built in the days where components sized themselves
        // raw ComboBox doesn't do that
        trackCombo->setSize(100, 20);

        // this will also delete the former trackSelector radio
        FormPanel* formPanel = form.getPanel("Tracks");
        formPanel->replaceHeader(trackCombo);
        trackSelector = nullptr;
    }

    if (trackSelector != nullptr)
      trackSelector->setSelection(selectedTrack);
    else if (trackCombo != nullptr)
      trackCombo->setSelectedId(selectedTrack + 1);
#endif
}

/**
 * Called by the Save button in the footer.
 * 
 * Save all setups that have been edited during this session
 * back to the master configuration.
 *
 * Tell the ConfigEditor we are done.
 */
void MidiTrackEditor::save()
{
    // copy visible state back into the object
    // need to also do this when the selected setup is changed
    saveSession();

    Session* master = supervisor->getSession();
    // transfer the track count and the track definitions
    master->midiTracks = session->midiTracks;
    master->clearTracks(Session::TypeMidi);
    for (int i = 0 ; i < session->tracks.size() ; i++) {
        Session::Track* t = session->tracks[i];
        if (t->type == Session::TypeMidi) {
            (void)session->tracks.removeAndReturn(i);
            master->tracks.add(t);
        }
    }

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
    loadTrack(0);
}

void MidiTrackEditor::loadTrack(int index)
{
    Session::Track* track = session->getTrack(Session::TypeMidi, index);
    if (track != nullptr) {
        form.load(track->parameters.get());
    }
    else {
        // didn't have a definition for this one, reset the fields to initial values
        form.load(nullptr);
    }
}

void MidiTrackEditor::saveSession()
{
    session->midiTracks = trackCount.getInt();
    saveTrack(0);
}

void MidiTrackEditor::saveTrack(int index)
{
    Session::Track* track = session->ensureTrack(Session::TypeMidi, index);
    if (track->parameters == nullptr)
      track->parameters.reset(new ValueSet());
    form.save(track->parameters.get());
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
    Trace(2, "MidiTrackEditor: Track selected %d", index);
}

void MidiTrackEditor::comboSelected(class YanCombo* c, int index)
{
    (void)c;
    Trace(2, "MidiTrackEditor: Sync source selected %d", index);
}

void MidiTrackEditor::inputChanged(class YanInput* input)
{
    Trace(2, "MidiTrackEditor: Track count changed %d", input->getInt());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
