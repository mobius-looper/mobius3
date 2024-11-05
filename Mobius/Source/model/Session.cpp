
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "ValueSet.h"
#include "Session.h"

Session::Session()
{
}

Session::~Session()
{
}

Session::Session(Session* src)
{
    audioTracks = src->audioTracks;
    midiTracks = src->midiTracks;
    
    for (auto track : src->tracks) {
        tracks.add(new Track(track));
    }

    if (src->globals != nullptr)
      globals.reset(new ValueSet(src->globals.get()));
}

/**
 * Return the definition of this track if we have one.
 */
Session::Track* Session::getTrack(TrackType type, int index)
{
    Track* found = nullptr;
    int count = 0;
    for (auto track : tracks) {
        if (track->type == type) {
            if (count == index) {
                found = track;
                break;
            }
            count++;
        }
    }
    return found;
}

/**
 * Find or create a definition for this track index.
 * Because the indexes are expected to match array indexes,
 * need to flesh out preceeding tracks if they don't exist.
 *
 * todo: I don't like the way this is working.  Everywhere else tracks
 * just have a unique number and may be of either type.
 */
Session::Track* Session::ensureTrack(TrackType type, int index)
{
    Track* found = nullptr;

    int count = 0;
    for (auto track : tracks) {
        if (track->type == type) {
            if (count == index) {
                found = track;
                break;
            }
            count++;
        }
    }

    if (found == nullptr) {
        for (int i = count ; i <= index ; i++) {
            found = new Session::Track();
            found->index = i;
            found->type = type;
            tracks.add(found);
        }
    }

    return found;
}

/**
 * Move the tracks from one session to another.
 * Used by MidiTrackEditor
 */
void Session::replaceMidiTracks(Session* src)
{
    clearTracks(TypeMidi);
    
    int index = 0;
    while (index < src->tracks.size()) {
        Session::Track* t = src->tracks[index];
        if (t->type == Session::TypeMidi) {
            (void)src->tracks.removeAndReturn(index);
            tracks.add(t);
        }
        else {
            index++;
        }
    }

    // this is authoritative over how many tracks there logically are
    // the Track array may be sparse or have extras
    midiTracks = src->midiTracks;
}


void Session::clearTracks(TrackType type)
{
    int index = 0;
    while (index < tracks.size()) {
        Track* t = tracks[index];
        if (t->type == type) {
            (void)tracks.remove(index, true);
        }
        else
          index++;
    }
}

ValueSet* Session::getGlobals()
{
    return globals.get();
}

ValueSet* Session::ensureGlobals()
{
    if (globals == nullptr)
      globals.reset(new ValueSet());
    return globals.get();
}

MslValue* Session::get(juce::String pname)
{
    MslValue* v = nullptr;
    if (globals != nullptr)
      v = globals->get(pname);
    return v;
}

bool Session::getBool(juce::String pname)
{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getBool() : false;
}

int Session::getInt(juce::String pname)

{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getInt() : false;
}

const char* Session::getString(juce::String pname)
{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getString() : nullptr;
}

void Session::setString(juce::String pname, const char* value)
{
    ValueSet* g = ensureGlobals();
    g->setString(pname, value);
}

void Session::setJString(juce::String pname, juce::String value)
{
    ValueSet* g = ensureGlobals();
    g->setJString(pname, value);
}

void Session::setInt(juce::String pname, int value)
{
    ValueSet* g = ensureGlobals();
    g->setInt(pname, value);
}    

void Session::setBool(juce::String pname, bool value)
{
    ValueSet* g = ensureGlobals();
    g->setBool(pname, value);
}    

//////////////////////////////////////////////////////////////////////
//
// Track
//
//////////////////////////////////////////////////////////////////////

Session::Track::Track(Session::Track* src)
{
    type = src->type;
    name = src->name;
    index = src->index;
    if (src->parameters != nullptr)
      parameters.reset(new ValueSet(src->parameters.get()));
}

ValueSet* Session::Track::getParameters()
{
    return parameters.get();
}

ValueSet* Session::Track::ensureParameters()
{
    if (parameters == nullptr)
      parameters.reset(new ValueSet());
    return parameters.get();
}

MslValue* Session::Track::get(juce::String pname)
{
    MslValue* v = nullptr;
    if (parameters != nullptr)
      v = parameters->get(pname);
    return v;
}

bool Session::Track::getBool(juce::String pname)
{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getBool() : false;
}

int Session::Track::getInt(juce::String pname)

{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getInt() : false;
}

const char* Session::Track::getString(juce::String pname)
{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getString() : nullptr;
}

//////////////////////////////////////////////////////////////////////
//
// XML
//
//////////////////////////////////////////////////////////////////////

void Session::parseXml(juce::String xml)
{
    juce::XmlDocument doc(xml);
    std::unique_ptr<juce::XmlElement> root = doc.getDocumentElement();
    if (root == nullptr) {
        xmlError("Parse parse error: %s\n", doc.getLastParseError());
    }
    else if (!root->hasTagName("Session")) {
        xmlError("Unexpected XML tag name: %s\n", root->getTagName());
    }
    else {
        audioTracks = root->getIntAttribute("audioTracks");
        midiTracks = root->getIntAttribute("midiTracks");
        
        for (auto* el : root->getChildIterator()) {
            if (el->hasTagName(ValueSet::XmlElement)) {
                ValueSet* set = new ValueSet();
                set->parse(el);
                globals.reset(set);
            }
            else if (el->hasTagName("Track")) {
                tracks.add(parseTrack(el));
            }
            else {
                Trace(1, "Session: Invalid XML element %s", el->getTagName().toUTF8());
            }
        }

        // re-index the tracks, looks better in the debugger
        int index = 0;
        for (auto track : tracks) {
            track->index = index;
            index++;
        }
    }
}

void Session::xmlError(const char* msg, juce::String arg)
{
    juce::String fullmsg ("Session: " + juce::String(msg));
    if (arg.length() == 0)
      Trace(1, fullmsg.toUTF8());
    else
      Trace(1, fullmsg.toUTF8(), arg.toUTF8());
}

Session::Track* Session::parseTrack(juce::XmlElement* root)
{
    Session::Track* track = new Session::Track();

    // todo: should be "id" ?
    track->name = root->getStringAttribute("name");
    
    juce::String typeString = root->getStringAttribute("type");
    if (typeString == juce::String("audio")) {
        track->type = Session::TypeAudio;
    }
    else if (typeString == juce::String("midi")) {
        track->type = Session::TypeMidi;
    }
    else {
        xmlError("Invalid track type %s", typeString);
    }
    
    for (auto* el : root->getChildIterator()) {

        if (el->hasTagName(ValueSet::XmlElement)) {
            ValueSet* set = track->ensureParameters();
            set->parse(el);
        }
        else if (el->hasTagName("MidiDevice")) {
            SessionMidiDevice* device = new SessionMidiDevice();
            parseDevice(el, device);
            track->devices.add(device);
        }
        else {
            xmlError("Invalid XML element %s", el->getTagName().toUTF8());
        }

    }
    return track;
}

juce::String Session::toXml()
{
    juce::XmlElement root ("Session");

    if (audioTracks > 0)
      root.setAttribute("audioTracks", audioTracks);
    
    if (midiTracks > 0)
      root.setAttribute("midiTracks", midiTracks);

    for (auto track : tracks)
      renderTrack(&root, track);

    if (globals != nullptr)
      globals->render(&root);

    return root.toString();
}

void Session::renderTrack(juce::XmlElement* parent, Session::Track* track)
{
    juce::XmlElement* root = new juce::XmlElement("Track");
    parent->addChildElement(root);

    if (track->type == Session::TypeAudio)
      root->setAttribute("type", "audio");
    else if (track->type == Session::TypeMidi)
      root->setAttribute("type", "midi");
    else
      root->setAttribute("type", "???");

    root->setAttribute("index", track->index);

    if (track->name.length() > 0)
      root->setAttribute("name", track->name);
    
    ValueSet* params = track->getParameters();
    if (params != nullptr)
      params->render(root);

    for (auto device : track->devices)
      renderDevice(root, device);
}

void Session::renderDevice(juce::XmlElement* parent, SessionMidiDevice* device)
{
    juce::XmlElement* root = new juce::XmlElement("MidiDevice");
    parent->addChildElement(root);

    root->setAttribute("name", device->name);
    
    if (device->record)
      root->setAttribute("record", device->record);
    
    if (device->id > 0)
      root->setAttribute("id", device->id);
    
    if (device->output.length() > 0)
      root->setAttribute("output", device->output);
}

void Session::parseDevice(juce::XmlElement* root, SessionMidiDevice* device)
{
    device->name = root->getStringAttribute("name");
    device->record = root->getBoolAttribute("record");
    device->id = root->getIntAttribute("id");
    device->output = root->getStringAttribute("output");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
