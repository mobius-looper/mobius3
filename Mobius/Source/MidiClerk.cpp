
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
    // analyzeFile(file);
    MidiSequence* seq = toSequence(file);
    if (seq != nullptr) {
        MobiusInterface* mobius = supervisor->getMobius();
        mobius->loadMidiLoop(seq, 0, 0);
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

        if (ntracks > 1) {
            Trace(1, "MidiClerk: More than one track in file");
            // unclear what we would do with this, I guess if we had
            // a MidiFile wrapper we could load multiple sequences and return all of them
        }
    }

    if (heldNotes != nullptr)
      Trace(1, "MidiClerk: Lingering held notes after reading file");

    return sequence;
}

void MidiClerk::toSequence(const juce::MidiMessageSequence* mms, MidiSequence* seq)
{
    tsigNumerator = 0;
    tsigDenominator = 0;
    secondsPerQuarter = 0.0f;
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
                seq->add(on);
            }
        }
        else if (msg.isMetaEvent()) {
            if (msg.isTempoMetaEvent()) {
                // don't need this?
                //double tickLength = msg.getTempoMetaEventTickLength((short)timeFormat);
                secondsPerQuarter = msg.getTempoSecondsPerQuarterNote();
            }
            else if (msg.isTimeSignatureMetaEvent()) {
                msg.getTimeSignatureInfo(tsigNumerator, tsigDenominator);
            }
        }
        else {
            Trace(1, "MidiClerk: Event in file was not a note on/off");
        }
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
