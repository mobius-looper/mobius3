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
        initInputDevice(track);
        initOutputDevice(track);
        // adapt to changes in the midi device since the last time
        form.resized();
    }
    else {
        // didn't have a definition for this one, reset the fields to initial values
        form.load(nullptr);
    }
}

void MidiTrackEditor::initInputDevice(Session::Track* track)
{
    MidiManager* mm = supervisor->getMidiManager();
    juce::StringArray names = mm->getOpenInputDevices();
    if (supervisor->isPlugin())
      names.insert(0, "Host");
    else
      names.insert(0, "Any");
    inputDevice.setItems(names);

    int index = 0;
    ValueSet* params = track->getParameters();
    if (params != nullptr) {
        const char* savedName = params->getString("inputDevice");
        if (savedName != nullptr) {
            index = names.indexOf(juce::String(savedName));
            if (index < 0) {
                Trace(1, "MidiTrackEditor: Saved track input device not available %s", savedName);
                index = 0;
            }
        }
    }
    inputDevice.setSelection(index);
}

void MidiTrackEditor::initOutputDevice(Session::Track* track)
{
    MidiManager* mm = supervisor->getMidiManager();
    juce::StringArray names = mm->getOpenOutputDevices();
    if (supervisor->isPlugin())
      names.insert(0, "Host");
    // output device defaults to the first one
    outputDevice.setItems(names);

    int index = 0;
    ValueSet* params = track->getParameters();
    if (params != nullptr) {
        const char* savedName = params->getString("outputDevice");
        if (savedName != nullptr) {
            index = names.indexOf(juce::String(savedName));
            if (index < 0) {
                Trace(1, "MidiTrackEditor: Saved track output device not available %s", savedName);
                index = 0;
            }
        }
    }
    outputDevice.setSelection(index);
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


    juce::String devname = inputDevice.getSelectionText();
    if (devname == "Any") {
        // don't save this
        params->setString("inputDevice", nullptr);
    }
    else
      params->setJString("inputDevice", inputDevice.getSelectionText());
    
    params->setJString("outputDevice", outputDevice.getSelectionText());
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
    form.addSpacer();
    
    form.add(&inputDevice);
    form.add(&outputDevice);

    form.addField(ParamSyncSource);
    form.addField(ParamTrackSyncUnit);
    form.addField(ParamSlaveSyncUnit);
    form.addField(ParamBeatsPerBar);
    form.addField(ParamLoopCount);
    
    
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
