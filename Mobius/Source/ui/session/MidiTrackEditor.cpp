/**
 * ConfigEditor for the MIDI tracks.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Session.h"
#include "../../model/UIConfig.h"
#include "../../model/MobiusConfig.h"
#include "../../Supervisor.h"

#include "../common/SimpleRadio.h"

#include "MidiTrackEditor.h"

MidiTrackEditor::MidiTrackEditor(Supervisor* s) : ConfigEditor(s), generalForm(s), switchForm(s), followerForm(s)
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
    juce::Rectangle<int> area = getLocalBounds();
    rootForm.setBounds(area.removeFromTop(rootForm.getPreferredHeight()));

    tabs.setBounds(area);
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

    UIConfig* config = supervisor->getUIConfig();
    juce::String favorites = config->get("symbolTreeFavorites");
    
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
    // before we used getTrack which might return nullptr
    // now use ensureTrack and assume it is initialized to reasonable values
    // effect should be the same
    // Session::Track* track = session->getTrack(Session::TypeMidi, index);
    Session::Track* track = session->ensureTrack(Session::TypeMidi, index);
    
    if (track != nullptr) {
        
        generalForm.load(track->getParameters());
        initInputDevice(track);
        initOutputDevice(track);
        initTrackGroup(track);
        midiThru.setValue(track->getBool("midiThru"));
        // adapt to changes in the midi device since the last time
        generalForm.resized();

        followerForm.load(track->getParameters());
        switchForm.load(track->getParameters());
    }
    else {
        // didn't have a definition for this one, reset the fields to initial values
        generalForm.load(nullptr);
        followerForm.load(nullptr);
        switchForm.load(nullptr);
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
    const char* savedName = track->getString("inputDevice");
    if (savedName != nullptr) {
        index = names.indexOf(juce::String(savedName));
        if (index < 0) {
            // soften the level since this can hit the trace breakpoint
            // every time the window comes up
            // todo: should show something in the editor so they know
            Trace(2, "MidiTrackEditor: Warning: Saved track input device not available %s", savedName);
            index = 0;
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
    const char* savedName = track->getString("outputDevice");
    if (savedName != nullptr) {
        index = names.indexOf(juce::String(savedName));
        if (index < 0) {
            Trace(2, "MidiTrackEditor: Warning Saved track output device not available %s", savedName);
            index = 0;
        }
    }
    outputDevice.setSelection(index);
}

void MidiTrackEditor::initTrackGroup(Session::Track* track)
{
    MobiusConfig* config = supervisor->getMobiusConfig();
    juce::StringArray names;

    names.add("[None]");
    for (auto def : config->groups)
      names.add(def->name);

    trackGroup.setItems(names);

    juce::String current = juce::String(track->getString("group"));
    int ordinal = config->getGroupOrdinal(juce::String(current));
    // ordinal is -1 if not found, which matches [None]
    trackGroup.setSelection(ordinal + 1);
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
    generalForm.save(params);
    followerForm.save(params);
    switchForm.save(params);
    
    juce::String devname = inputDevice.getSelectionText();
    if (devname == "Any") {
        // don't save this
        params->setString("inputDevice", nullptr);
    }
    else
      params->setJString("inputDevice", inputDevice.getSelectionText());
    
    params->setJString("outputDevice", outputDevice.getSelectionText());

    params->setBool("midiThru", midiThru.getValue());

    int ordinal = trackGroup.getSelection();
    if (ordinal <= 0) {
        // none or unselected
        params->setString("group", nullptr);
    }
    else {
        params->setJString("group", trackGroup.getSelectionText());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Form Rendering
//
//////////////////////////////////////////////////////////////////////

void MidiTrackEditor::render()
{
    trackCount.setListener(this);
    rootForm.add(&trackCount);
    rootForm.addSpacer();
    
    trackSelector.setButtonCount(8);
    trackSelector.setListener(this);
    rootForm.add(&trackSelector);
    rootForm.addSpacer();
    
    addAndMakeVisible(rootForm);
    
    generalForm.add(&inputDevice);
    generalForm.add(&outputDevice);
    generalForm.add(&midiThru);
    generalForm.addField(ParamMidiChannelOverride);
    generalForm.addSpacer();
    
    generalForm.addField(ParamSyncSource);
    generalForm.addField(ParamTrackSyncUnit);
    generalForm.addField(ParamSlaveSyncUnit);
    generalForm.addField(ParamBeatsPerBar);
    generalForm.addSpacer();
    
    generalForm.addField(ParamLoopCount);
    generalForm.addField(ParamQuantize);
    generalForm.addField(ParamSubcycles);
    generalForm.add(&trackGroup);
        
    tabs.add("General", &generalForm);

    followerForm.addField(ParamLeaderType);
    followerForm.addField(ParamLeaderTrack);
    followerForm.addField(ParamFollowRecord);
    followerForm.addField(ParamFollowRecordEnd);
    followerForm.addField(ParamFollowerMuteStart);
    followerForm.addField(ParamFollowMute);
    followerForm.addField(ParamLeaderSwitchLocation);
    followerForm.addField(ParamFollowQuantizeLocation);
    
    //followerForm.addField(ParamFollowSize);
    //followerForm.addField(ParamFollowLocation);
    
    tabs.add("Follower", &followerForm);

    switchForm.addField(ParamSwitchQuantize);
    switchForm.addField(ParamEmptyLoopAction);
    switchForm.addField(ParamSwitchDuration);
    switchForm.addField(ParamSwitchLocation);

    tabs.add("Loop Switch", &switchForm);

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
