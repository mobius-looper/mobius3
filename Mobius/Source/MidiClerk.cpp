
#include <JuceHeader.h>

#include "util/Trace.h"
#include "midi/MidiEvent.h"
#include "midi/MidiSequence.h"
#include "mobius/MobiusInterface.h"

#include "Supervisor.h"

#include "MidiClerk.h"


MidiClerk::MidiClerk(Supervisor* s)
{
    supervisor = s;
}

MidiClerk::~MidiClerk()
{
}

//////////////////////////////////////////////////////////////////////
//
// Load
//
//////////////////////////////////////////////////////////////////////

/**
 * Load a MIDI file into the active loop of the foused track.
 */
void MidiClerk::loadFile()
{
    MobiusView* view = supervisor->getMobiusView();
    // focusedTrack is an index
    if (view->focusedTrack < view->audioTracks) {
        supervisor->alert("MIDI Track must have focus");
    }
    else {
        // is it safe to save state here or would it be better to pass it
        // into the choose closure?
        destinationTrack = view->focusedTrack + 1;
        destinationLoop = 0;
        analyze = false;
        chooseMidiFile();
    }
}

void MidiClerk::analyzeFile()
{
    MobiusView* view = supervisor->getMobiusView();
    // focusedTrack is an index
    if (view->focusedTrack < view->audioTracks) {
        supervisor->alert("MIDI Track must have focus");
    }
    else {
        // is it safe to save state here or would it be better to pass it
        // into the choose closure?
        destinationTrack = view->focusedTrack + 1;
        destinationLoop = 0;
        analyze = true;
        chooseMidiFile();
    }
}

/**
 * Load a file into a loop clicked on in the loop stack.
 */
void MidiClerk::loadFile(int trackNumber, int loopNumber)
{
    MobiusView* view = supervisor->getMobiusView();
    if (trackNumber <= view->audioTracks) {
        supervisor->alert("Track is not a MIDI track");
    }
    else {
        destinationTrack = trackNumber;
        destinationLoop = loopNumber;
        analyze = false;
        chooseMidiFile();
    }
}

void MidiClerk::chooseMidiFile()
{
    juce::File startPath(supervisor->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;

    juce::String title = "Select a MIDI file...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.mid");

    auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            // chooserFlags should have only allowed one
            juce::File file = result[0];
            
            doFileLoad(file);

            // remember this directory for the next time
            lastFolder = file.getParentDirectory().getFullPathName();
        }
        
    });
}

void MidiClerk::doFileLoad(juce::File file)
{
    Trace(2, "MidiClerk: Selected file %s", file.getFullPathName().toUTF8());

    if (analyze) {
        analyzeFile(file);
    }
    else {
        MobiusView* view = supervisor->getMobiusView();

        if (destinationTrack <= view->audioTracks) {
            // back track number or sholdn't have asked for MIDI
            supervisor->alert("MIDI track must have focus");
        }
        else {
            MidiSequence* seq = toSequence(file);
            if (seq != nullptr) {
                MobiusInterface* mobius = supervisor->getMobius();
                // leave loop unspecified, it goes to the active loop
                mobius->loadMidiLoop(seq, destinationTrack, destinationLoop);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Drag In
//
//////////////////////////////////////////////////////////////////////

/**
 * Here indirectly from AudioClerk since the UI doesn't understand us yet.
 * Need a more generic file distributor.
 * At this point we will have already filtered out just .mid and .smf files
 * But we need to check the target track type.
 */
void MidiClerk::filesDropped(const juce::StringArray& files, int track, int loop)
{
    MobiusView* view = supervisor->getMobiusView();
    if (track == 0)
      track = view->focusedTrack + 1;

    // track is a 1 based number
    if (track <= view->audioTracks) {
        // either we dropped over an audio track or the focused track is an audio
        // track, no can do
        supervisor->alert("MIDI file dropped over audio track");
    }
    else if (files.size() > 0) {
        // todo: multiples
        juce::String path = files[0];
        juce::File file(path);

        MidiSequence* seq = toSequence(file);
        if (seq != nullptr) {
            MobiusInterface* mobius = supervisor->getMobius();
            // leave loop unspecified, it goes to the active loop
            mobius->loadMidiLoop(seq, track, loop);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Save
//
//////////////////////////////////////////////////////////////////////

void MidiClerk::saveFile()
{
    MobiusView* view = supervisor->getMobiusView();
    // focusedTrack is an index
    if (view->focusedTrack < view->audioTracks) {
        supervisor->alert("MIDI Track must have focus");
    }
    else {
        destinationTrack = view->focusedTrack + 1;
        destinationLoop = 0;
        chooseMidiSaveFile();
    }
}

void MidiClerk::saveFile(int trackNumber, int loopNumber)
{
    MobiusView* view = supervisor->getMobiusView();
    if (trackNumber <= view->audioTracks) {
        supervisor->alert("Track is not a MIDI track");
    }
    else {
        destinationTrack = trackNumber;
        destinationLoop = loopNumber;
        chooseMidiSaveFile();
    }
}

void MidiClerk::chooseMidiSaveFile()
{
    juce::File startPath(supervisor->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;

    juce::String title = "Select a MIDI loop destination...";

    chooser = std::make_unique<juce::FileChooser> (title, startPath, "*.mid");

    auto chooserFlags = juce::FileBrowserComponent::saveMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            // chooserFlags should have only allowed one
            juce::File file = result[0];
            
            doFileSave(file);

            // remember this directory for the next time
            lastFolder = file.getParentDirectory().getFullPathName();
        }
        
    });
}

void MidiClerk::doFileSave(juce::File file)
{
    Trace(2, "MidiClerk: Selected file %s", file.getFullPathName().toUTF8());

    MobiusView* view = supervisor->getMobiusView();

    if (destinationTrack <= view->audioTracks) {
        // bad track number or sholdn't have asked for MIDI
        supervisor->alert("MIDI track must have focus");
    }
    else {

        Trace(1, "MidiClerk: File save not implemented");

        // something like this
#if 0
        MobiusInterface* mobius = supervisor->getMobius();
        MidiSequence* seq = mobius->saveMidiLoop(destinationTrack, destinationLoop);
        if (seq != nullptr) {
            // do the other direction
        }
#endif        
    }
}

//////////////////////////////////////////////////////////////////////
//
// Drag Out
//
//////////////////////////////////////////////////////////////////////

void MidiClerk::dragOut(int trackNumber, int loopNumber)
{
    (void)trackNumber;
    (void)loopNumber;

    // this is what I had prototyped in LoopStack
    
#if 0    
    // use e.mods.isLeftButtonDown or isRightButtonDown if you want to distinguish buttons
    Trace(2, "StripLoopStack: Alt-Mouse %s down over loop %d", bstr, target);
    MobiusViewTrack* track = strip->getTrackView();

    // save the current loop in a temporary file
    juce::TemporaryFile* tfile = new juce::TemporaryFile(".mid", 0);
    juce::StringArray errors = strip->getProvider()->getMobius()->saveLoop(tfile->getFile());
    if (errors.size() > 0) {
        Trace(1, "StripElements: Save loop had errors");
    }
    juce::StringArray paths;
    paths.add(tfile->getFile().getFullPathName());
    // second arg is "canMoveFiles", since Supervisor is going to hold onto these
    // and clean them up at shutdown, not sure if moving them would mess that up
    bool status = juce::DragAndDropContainer::performExternalDragDropOfFiles(paths, false);
    if (!status) {
        Trace(1, "LoopStack: Failed to start external drag and drop");
        delete tfile;
    }
    else {
        strip->getProvider()->addTemporaryFile(tfile);
    }
#endif    

}

//////////////////////////////////////////////////////////////////////
//
// MidiSequence Conversion
//
//////////////////////////////////////////////////////////////////////

MidiSequence* MidiClerk::toSequence(juce::File file)
{
    MidiSequence* sequence = nullptr;
    juce::FileInputStream fstream(file);
    juce::MidiFile mfile;
    
    tsigNumerator = 0;
    tsigDenominator = 0;
    secondsPerQuarter = 0.0f;
    highest = 0.0f;

    heldNotes = nullptr;
    
    bool status = mfile.readFrom(fstream);
    if (!status)
      Trace(2, "MidiClerk: File could not be read");
    else {
        mfile.convertTimestampTicksToSeconds();

        int ntracks = mfile.getNumTracks();
        //int timeFormat =  mfile.getTimeFormat();

        if (ntracks == 0) {
            Trace(1, "MidiClerk: File has no tracks");
        }
        else {
            if (ntracks > 1)
              Trace(2, "MidiClerk: Warning: More than one track in file, merging");
                
            sequence = new MidiSequence();
            
            for (int i = 0 ; i < ntracks ; i++) {
                const juce::MidiMessageSequence* mms = mfile.getTrack(i);
                double last = toSequence(mms, sequence, true);
                if (last > highest)
                  highest = last;
                
                if (heldNotes != nullptr) {
                    Trace(1, "MidiClerk: Lingering held notes after reading track");
                    // todo: should force them off
                }
            }

            finalizeSequence(sequence, highest);
        }
    }

    return sequence;
}

double MidiClerk::toSequence(const juce::MidiMessageSequence* mms, MidiSequence* seq,
                             bool merge)
{
    double last = 0.0f;
    int sampleRate = supervisor->getSampleRate();
    
    for (int i = 0 ; i < mms->getNumEvents() ; i++) {
        juce::MidiMessageSequence::MidiEventHolder* holder = mms->getEventPointer(i);
        juce::MidiMessage& msg = holder->message;
        
        last = msg.getTimeStamp();
        
        if (msg.isNoteOn()) {
            MidiEvent* e = new MidiEvent();
            e->juceMessage = msg;

            double timestamp = msg.getTimeStamp();
            e->frame = (int)((double)sampleRate * timestamp);

            e->next = heldNotes;
            heldNotes = e;
        }
        else if (msg.isNoteOff()) {
            MidiEvent* on = findNoteOn(msg);
            if (on == nullptr) {
                Trace(1, "MidiClerk: Mismatched NoteOff");
            }
            else {
                double timestamp = msg.getTimeStamp();
                int endFrame = (int)((double)sampleRate * timestamp);
                on->duration = endFrame - on->frame;
                if (merge)
                  seq->insert(on);
                else
                  seq->add(on);
            }
        }
        else if (msg.isMetaEvent()) {
            if (msg.isTempoMetaEvent()) {
                // don't need this?
                //double tickLength = msg.getTempoMetaEventTickLength((short)timeFormat);
                if (secondsPerQuarter != 0.0f)
                  Trace(1, "MidiClerk: Redefining secondsPerQuarter");
                secondsPerQuarter = msg.getTempoSecondsPerQuarterNote();
            }
            else if (msg.isTimeSignatureMetaEvent()) {
                if (tsigNumerator != 0 || tsigDenominator != 0)
                  Trace(1, "MidiClerk: Redefining time signature");
                msg.getTimeSignatureInfo(tsigNumerator, tsigDenominator);
            }
        }
        else {
            Trace(1, "MidiClerk: Event in file was not a note on/off");
        }
    }

    if (!merge)
      finalizeSequence(seq, last);

    return last;
}

void MidiClerk::finalizeSequence(MidiSequence* seq, double last)
{
    int sampleRate = supervisor->getSampleRate();
    
    if (tsigNumerator <= 0 || tsigDenominator <= 0) {
        Trace(1, "MidiClerk: Unspecified or invalid time signature %d/%d", tsigNumerator, tsigDenominator);
        if (tsigNumerator <= 0)
          tsigNumerator = 4;
        if (tsigDenominator <= 0)
          tsigDenominator = 4;
    }

    if (secondsPerQuarter <= 0.0f) {
        Trace(1, "MidiClerk: Unspecified or invalid secondsPerQuarter %d", (int)secondsPerQuarter);
        secondsPerQuarter = 0.5f;
    }
    
    double quartersPerMeasure = (double)tsigNumerator / ((double)tsigDenominator / 4.0f);
    double secondsPerMeasure = quartersPerMeasure * secondsPerQuarter;
    
    int endMeasure = (int)(ceil(last / secondsPerMeasure));
    double trackEnd = endMeasure * secondsPerMeasure;
    int endFrame = (int)((double)sampleRate * trackEnd);

    seq->setTotalFrames(endFrame);
}

MidiEvent* MidiClerk::findNoteOn(juce::MidiMessage& msg)
{
    MidiEvent* found = nullptr;
    MidiEvent* prev = nullptr;

    for (MidiEvent*e = heldNotes ; e != nullptr ; e = e->next) {
        if (e->juceMessage.getNoteNumber() == msg.getNoteNumber() &&
            e->juceMessage.getChannel() == msg.getChannel()) {
            found = e;
            break;
        }
        prev = e;
    }

    if (found != nullptr) {
        if (prev == nullptr)
          heldNotes = found->next;
        else
          prev->next = found->next;
        found->next = nullptr;
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// File Analysis
//
//////////////////////////////////////////////////////////////////////

void MidiClerk::analyzeFile(juce::File file)
{
    juce::FileInputStream fstream(file);
    juce::MidiFile mfile;

    bool status = mfile.readFrom(fstream);
    if (!status)
      Trace(2, "MidiClerk: File could not be read");
    else {
        mfile.convertTimestampTicksToSeconds();

        juce::String buffer;

        int ntracks = mfile.getNumTracks();
        buffer += "File has " + juce::String(ntracks) + " tracks, ";
        int timeFormat =  mfile.getTimeFormat();
        buffer += "time format " + juce::String(timeFormat) + "\n";
        for (int i = 0 ; i < ntracks ; i++) {
            const juce::MidiMessageSequence* seq = mfile.getTrack(i);
            analyzeTrack(i, timeFormat, seq, buffer);
        }

        juce::File root = supervisor->getRoot();
        juce::File outfile = root.getChildFile("midifile.txt");
        outfile.replaceWithText(buffer);
    }
}

void MidiClerk::analyzeTrack(int track, int timeFormat, const juce::MidiMessageSequence* seq, juce::String& buffer)
{
    tsigNumerator = 0;
    tsigDenominator = 0;
    secondsPerQuarter = 0.0f;
    double last = 0.0f;
    
    buffer += "Track " + juce::String(track) + " has " + juce::String(seq->getNumEvents()) +
        " events\n";

    for (int i = 0 ; i < seq->getNumEvents() ; i++) {
        juce::MidiMessageSequence::MidiEventHolder* holder = seq->getEventPointer(i);
        if (holder->message.isMetaEvent()) {
            analyzeMetaEvent(holder->message, timeFormat, buffer);
        }
        else {
            buffer += juce::String(holder->message.getTimeStamp()) + ": ";
            if (holder->message.isMidiMachineControlMessage()) {
                buffer += ": MIDI Machine Control\n";
            }
            else {
                buffer += ": " + holder->message.getDescription() + "\n";
            }
        }
        last = holder->message.getTimeStamp();
    }

    if (tsigNumerator <= 0 || tsigDenominator <= 0) {
        Trace(1, "MidiClerk: Unspecified or invalid time signature %d/%d", tsigNumerator, tsigDenominator);
        if (tsigNumerator <= 0)
          tsigNumerator = 4;
        if (tsigDenominator <= 0)
          tsigDenominator = 4;
    }

    if (secondsPerQuarter <= 0.0f) {
        Trace(1, "MidiClerk: Unspecified or invalid secondsPerQuarter %d", (int)secondsPerQuarter);
        secondsPerQuarter = 0.5f;
    }
    
    double quartersPerMeasure = (double)tsigNumerator / ((double)tsigDenominator / 4.0f);
    double secondsPerMeasure = quartersPerMeasure * secondsPerQuarter;
    
    int endMeasure = (int)(ceil(last / secondsPerMeasure));
    double trackLength = endMeasure * secondsPerMeasure;

    buffer += "Quarters per measure: " + juce::String(quartersPerMeasure) +
        " Seconds per measure: " + juce::String(secondsPerMeasure) +
        " End measure: " + juce::String(endMeasure) +
        " Track length: " + juce::String(trackLength) +
        "\n";
}

void MidiClerk::analyzeMetaEvent(juce::MidiMessage& msg, int timeFormat, juce::String& buffer)
{
    buffer += juce::String(msg.getTimeStamp()) + ": ";
    buffer += "MetaEvent type " + juce::String(msg.getMetaEventType()) + " datalen " +
        juce::String(msg.getMetaEventLength()) + " ";
    
    if (msg.isTrackMetaEvent()) {
        buffer += "Track\n";
    }
    else if (msg.isEndOfTrackMetaEvent()) {
        buffer += "EndOfTrack\n";
    }
    else if (msg.isTextMetaEvent()) {
        buffer += "Text: " + msg.getTextFromTextMetaEvent() + "\n";
    }
    else if (msg.isTrackNameEvent()) {
        buffer += "TrackName: " + msg.getTextFromTextMetaEvent() + "\n";
    }
    else if (msg.isTempoMetaEvent()) {
        double tickLength = msg.getTempoMetaEventTickLength((short)timeFormat);
        double spq = msg.getTempoSecondsPerQuarterNote();
        buffer += "Tempo: tickLength " + juce::String(tickLength) + " secondsPerQuarter " + juce::String(spq) +
            "\n";
        secondsPerQuarter = spq;
    }
    else if (msg.isTimeSignatureMetaEvent()) {
        msg.getTimeSignatureInfo(tsigNumerator, tsigDenominator);
        buffer += "TimeSignature: " + juce::String(tsigNumerator) + "/" + juce::String(tsigDenominator) + "\n";
    }
    else if (msg.isKeySignatureMetaEvent()) {
        int sharpsOrFlats = msg.getKeySignatureNumberOfSharpsOrFlats();
        bool major = msg.isKeySignatureMajorKey();
        const char* type = (major) ? "major" : "minor";
        buffer += "KeySignature: " + juce::String(type) + " " + juce::String(sharpsOrFlats) + "\n";
    }
    else if (msg.isMidiChannelMetaEvent()) {
        // would be interesting to know if channel events can change several times
        // why is channel a meta event and not embedded in the message?
        // spec says this is "MIDI Channel Prefix Assignment" whatever the hell that means
        int channel = msg.getMidiChannelMetaEventChannel();
        buffer += "MidiChannel: " + juce::String(channel) + "\n";
    }
    else {
        // there are several others including instrument name, copyright notice
        // https://ccrma.stanford.edu/~craig/14q/midifile/MidiFileFormat.html
        buffer += "Unknown meta event type\n";
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
