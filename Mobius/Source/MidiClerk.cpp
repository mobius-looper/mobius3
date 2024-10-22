
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

void MidiClerk::loadFile()
{
    // this does it's thing async then calls back to doFileChosen
    chooseMidiFile();
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
    //traceFile(file);
    //convertFile(file);

    MidiSequence* seq = toSequence(file);
    if (seq != nullptr) {
        MobiusInterface* mobius = supervisor->getMobius();
        mobius->loadMidiLoop(seq, 0, 0);
    }
}

//////////////////////////////////////////////////////////////////////
//
// File Trace
//
//////////////////////////////////////////////////////////////////////

void MidiClerk::traceFile(juce::File file)
{
    juce::FileInputStream fstream(file);
    juce::MidiFile mfile;

    bool status = mfile.readFrom(fstream);
    if (!status)
      Trace(2, "MidiClerk: File could not be read");
    else {
        mfile.convertTimestampTicksToSeconds();
        
        int ntracks = mfile.getNumTracks();
        Trace(2, "File has %d tracks", ntracks);
        int timeFormat =  mfile.getTimeFormat();
        for (int i = 0 ; i < ntracks ; i++) {
            const juce::MidiMessageSequence* seq = mfile.getTrack(i);
            traceTrack(i, timeFormat, seq);
        }
    }
}

void MidiClerk::traceTrack(int track, int timeFormat, const juce::MidiMessageSequence* seq)
{
    Trace(2, "Track %d has %d events", track, seq->getNumEvents());

    // not sure how the iterator works...
    // for (auto holder : seq) {

    int max = 20;
    for (int i = 0 ; i < seq->getNumEvents() && i < max ; i++) {
        juce::MidiMessageSequence::MidiEventHolder* holder = seq->getEventPointer(i);
        if (holder->message.isMetaEvent()) {
            traceMetaEvent(holder->message, timeFormat);
        }
        else {
            char buf[32];
            snprintf(buf, sizeof(buf), "%f", (float)holder->message.getTimeStamp());
            if (holder->message.isMidiMachineControlMessage()) {
                Trace(2, "%s: MIDI Machine Control", buf);
            }
            else {
                Trace(2, "%s: %s", buf, holder->message.getDescription().toUTF8());
            }
        }
    }
}

void MidiClerk::traceMetaEvent(juce::MidiMessage& msg, int timeFormat)
{
    Trace(2, "MetaEvent: type %d datalen %d", msg.getMetaEventType(), msg.getMetaEventLength());
    if (msg.isTrackMetaEvent()) {
        Trace(2, "Track");
    }
    else if (msg.isEndOfTrackMetaEvent()) {
        Trace(2, "EndOfTrack");
    }
    else if (msg.isTextMetaEvent()) {
        Trace(2, "Text %s", msg.getTextFromTextMetaEvent().toUTF8());
    }
    else if (msg.isTrackNameEvent()) {
        Trace(2, "TrackName %s", msg.getTextFromTextMetaEvent().toUTF8());
    }
    else if (msg.isTempoMetaEvent()) {
        double tickLength = msg.getTempoMetaEventTickLength((short)timeFormat);
        double spq = msg.getTempoSecondsPerQuarterNote();
        char buf[256];
        snprintf(buf, sizeof(buf), "tickLength %f secondsPerQuarter %f", (float)tickLength, (float)spq);
        Trace(2, "Tempo %s", buf);
    }
    else if (msg.isTimeSignatureMetaEvent()) {
        int numerator = 0;
        int denominator = 0;
        msg.getTimeSignatureInfo(numerator, denominator);
        Trace(2, "TimeSignature %d / %d", numerator, denominator);
    }
    else if (msg.isKeySignatureMetaEvent()) {
        int sharpsOrFlats = 0;
        msg.getKeySignatureNumberOfSharpsOrFlats();
        bool major = msg.isKeySignatureMajorKey();
        const char* type = (major) ? "major" : "minor";
        Trace(2, "KeySignature sharpsOrFlats %s %d", type, sharpsOrFlats);
    }
    else if (msg.isMidiChannelMetaEvent()) {
        int channel = msg.getMidiChannelMetaEventChannel();
        Trace(2, "MidiChannel %d", channel);
    }
    else {
        Trace(2, "Unknown meta event type?");
    }
}

//////////////////////////////////////////////////////////////////////
//
// File Conversion
//
//////////////////////////////////////////////////////////////////////

void MidiClerk::convertFile(juce::File file)
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
        buffer += "File has " + juce::String(ntracks) + " tracks\n";
        int timeFormat =  mfile.getTimeFormat();
        buffer += "Time format: " + juce::String(timeFormat) + "\n";
        for (int i = 0 ; i < ntracks ; i++) {
            const juce::MidiMessageSequence* seq = mfile.getTrack(i);
            convertTrack(i, timeFormat, seq, buffer);
        }
        
        juce::File root = supervisor->getRoot();
        juce::File outfile = root.getChildFile("midifile.txt");
        outfile.replaceWithText(buffer);
    }
}

void MidiClerk::convertTrack(int track, int timeFormat, const juce::MidiMessageSequence* seq, juce::String& buffer)
{
    buffer += "Track " + juce::String(track) + " has " + juce::String(seq->getNumEvents()) +
        " events\n";

    for (int i = 0 ; i < seq->getNumEvents() ; i++) {
        juce::MidiMessageSequence::MidiEventHolder* holder = seq->getEventPointer(i);
        if (holder->message.isMetaEvent()) {
            convertMetaEvent(holder->message, timeFormat, buffer);
        }
        else {
            char buf[32];
            snprintf(buf, sizeof(buf), "%f", (float)holder->message.getTimeStamp());
            if (holder->message.isMidiMachineControlMessage()) {
                buffer += buf;
                buffer += ": MIDI Machine Control\n";
            }
            else {
                buffer += buf;
                buffer += ": " + holder->message.getDescription() + "\n";
            }
        }
    }
}

void MidiClerk::convertMetaEvent(juce::MidiMessage& msg, int timeFormat, juce::String& buffer)
{
    buffer += "MetaEvent: type " + juce::String(msg.getMetaEventType()) + " datalen " +
        juce::String(msg.getMetaEventLength()) + "\n";
    
    if (msg.isTrackMetaEvent()) {
        buffer += "  Track\n";
    }
    else if (msg.isEndOfTrackMetaEvent()) {
        buffer += "  EndOfTrack\n";
    }
    else if (msg.isTextMetaEvent()) {
        buffer += "  Text " + msg.getTextFromTextMetaEvent();
    }
    else if (msg.isTrackNameEvent()) {
        buffer += "  TrackName " + msg.getTextFromTextMetaEvent();
    }
    else if (msg.isTempoMetaEvent()) {
        double tickLength = msg.getTempoMetaEventTickLength((short)timeFormat);
        double spq = msg.getTempoSecondsPerQuarterNote();
        buffer += "  Tempo tickLength " + juce::String(tickLength) + " secondsPerQuarter " + juce::String(spq) +
            "\n";
    }
    else if (msg.isTimeSignatureMetaEvent()) {
        int numerator = 0;
        int denominator = 0;
        msg.getTimeSignatureInfo(numerator, denominator);
        buffer += "  TimeSignature " + juce::String(numerator) + "/" + juce::String(denominator) + "\n";
    }
    else if (msg.isKeySignatureMetaEvent()) {
        int sharpsOrFlats = msg.getKeySignatureNumberOfSharpsOrFlats();
        bool major = msg.isKeySignatureMajorKey();
        const char* type = (major) ? "major" : "minor";
        buffer += "  KeySignature " + juce::String(type) + " " + juce::String(sharpsOrFlats) + "\n";
    }
    else if (msg.isMidiChannelMetaEvent()) {
        int channel = msg.getMidiChannelMetaEventChannel();
        buffer += "  MidiChannel " + juce::String(channel) + "\n";
    }
    else {
        buffer += "  Unknown meta event type?";
    }
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

    heldNotes = nullptr;
    
    bool status = mfile.readFrom(fstream);
    if (!status)
      Trace(2, "MidiClerk: File could not be read");
    else {
        mfile.convertTimestampTicksToSeconds();

        int ntracks = mfile.getNumTracks();
        //int timeFormat =  mfile.getTimeFormat();

        if (ntracks > 0) {
            const juce::MidiMessageSequence* mms = mfile.getTrack(0);
            sequence = new MidiSequence();
            toSequence(mms, sequence);
        }
    }

    if (heldNotes != nullptr)
      Trace(1, "MidiClerk: Lingering held notes after reading file");

    return sequence;
}

void MidiClerk::toSequence(const juce::MidiMessageSequence* mms, MidiSequence* seq)
{
    int sampleRate = supervisor->getSampleRate();
    
    for (int i = 0 ; i < mms->getNumEvents() ; i++) {
        juce::MidiMessageSequence::MidiEventHolder* holder = mms->getEventPointer(i);
        if (holder->message.isNoteOn()) {
            MidiEvent* e = new MidiEvent();
            e->juceMessage = holder->message;

            double timestamp = holder->message.getTimeStamp();
            e->frame = (int)((double)sampleRate * timestamp);

            e->next = heldNotes;
            heldNotes = e;
        }
        else if (holder->message.isNoteOff()) {
            MidiEvent* on = findNoteOn(holder->message);
            if (on == nullptr) {
                Trace(1, "MidiClerk: Mismatched NoteOff");
            }
            else {
                double timestamp = holder->message.getTimeStamp();
                int endFrame = (int)((double)sampleRate * timestamp);
                on->duration = endFrame - on->frame;
                seq->add(on);
            }
        }
        else if (!holder->message.isMetaEvent()) {
            Trace(1, "MidiClerk: Event in file was not a note on/off");
        }
    }
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

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
