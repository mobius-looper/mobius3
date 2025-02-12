
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
    id = src->id;
    version = src->version;
    
    for (auto track : src->tracks) {
        tracks.add(new Track(track));
    }

    if (src->globals != nullptr)
      globals.reset(new ValueSet(src->globals.get()));

    // source tracks should already have ids but make sure
    assignIds();
}

int Session::getId()
{
    return id;
}

void Session::setId(int i)
{
    id = i;
}

int Session::getVersion()
{
    return version;
}

void Session::setVersion(int v)
{
    version = v;
}

/**
 * Hack to pass both the Session and the MobiusConfig in one
 * KernelMessage payload to avoid ordering problems.
 */
void Session::setOldConfig(MobiusConfig* config)
{
    oldConfig = config;
}

MobiusConfig* Session::getOldConfig()
{
    return oldConfig;
}

/**
 * After parsing or editing, make sure all tracks have a unique id.
 */
void Session::assignIds()
{
    int max = 0;
    for (auto track : tracks) {
        if (track->id > max)
          max = track->id;
    }
    max++;
    for (auto track : tracks) {
        if (track->id == 0)
          track->id = max++;
    }
}

/**
 * This is going to be interesting.
 * This is mostly used in the Supervisor/Shell session to watch for changes
 * during upgrade, but since that object is copied and sent to the Kernel, we won't
 * know if the Kernel made any changes on shutdown.  Need more here.
 *
 * ?? where is this used
 */
void Session::setModified(bool b)
{
    modified = b;
}

bool Session::isModified()
{
    return modified;
}

juce::String Session::getName()
{
    return name;
}

void Session::setName(juce::String s)
{
    name = s;
}

juce::String Session::getLocation()
{
    return location;
}

void Session::setLocation(juce::String s)
{
    location = s;
}

//////////////////////////////////////////////////////////////////////
//
// Track Management
//
//////////////////////////////////////////////////////////////////////

int Session::getTrackCount()
{
    return tracks.size();
}

int Session::getAudioTracks()
{
    return countTracks(TypeAudio);
}

int Session::getMidiTracks()
{
    return countTracks(TypeMidi);
}

int Session::countTracks(TrackType type)
{
    int count = 0;
    for (auto track : tracks) {
        if (track->type == type) {
            count++;
        }
    }
    return count;
}

Session::Track* Session::getTrackByIndex(int index)
{
    Session::Track* track = nullptr;
    if (index >= 0 && index < tracks.size())
      track = tracks[index];
    return track;
}

Session::Track* Session::getTrackById(int otherid)
{
    Track* track = nullptr;
    for (int i = 0 ; i < tracks.size() ; i++) {
        Track* t = tracks[i];
        if (t->id == otherid) {
            track = t;
            break;
        }
    }
    return track;
}

/**
 * This is intended for ModelTransformer when merging old
 * Setups from MobiusConfig into a Session.  Each SetupTrack
 * ia paired with a Session::Track by it's position within the
 * set of TypeAudio tracks.
 */
Session::Track* Session::getTrackByType(TrackType type, int index)
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
 * Reconcile the number of tracks of a given type.
 * This is used in the SessionEditor when making bulk changes
 * to track counts, and by ScriptClerk/ModelTransformer during
 * the initial transition from Setups to Sessions.
 */
void Session::reconcileTrackCount(TrackType type, int required)
{
    // how many are there now?
    int currentCount = 0;
    for (int i = 0 ; i < tracks.size() ; i++) {
        Track* t = tracks[i];
        if (t->type == type) {
            currentCount++;
        }
    }

    if (currentCount < required) {
        // add new ones
        while (currentCount < required) {
            Track* neu = new Track();
            neu->type = type;
            tracks.add(neu);
            currentCount++;
        }
        modified = true;
    }
    else if (currentCount > required) {
        // awkward since they can be in random order
        // seek up to the position after the last track of this type
        int position = 0;
        int found = 0;
        while (position < tracks.size() && found < required) {
            Track* t = tracks[position];
            if (t->type == type)
              found++;
            position++;
        }
        // now delete the remainder
        while (position < tracks.size()) {
            Track* t = tracks[position];
            if (t->type == type)
              tracks.remove(position, true);
            else
              position++;
        }
        modified = true;
    }

    assignIds();
}

//////////////////////////////////////////////////////////////////////
//
// Session Editor
//
//////////////////////////////////////////////////////////////////////

/**
 * Used for use by SessionClerk during migration
 */
void Session::add(Track* t)
{
    if (t != nullptr)
      tracks.add(t);
}

void Session::deleteTrack(int index)
{
    Session::Track* t = getTrackByIndex(index);
    if (t != nullptr) {
        tracks.removeObject(t, true);
    }
}

void Session::move(int sourceIndex, int desiredIndex)
{
    if (sourceIndex != desiredIndex &&
        sourceIndex >= 0 && sourceIndex < tracks.size() &&
        desiredIndex >= 0 && desiredIndex < tracks.size()) {

        tracks.move(sourceIndex, desiredIndex);
    }
}

void Session::steal(juce::Array<Track*>& dest)
{
    dest.clear();
    while (tracks.size() > 0) {
        Track* t = tracks.removeAndReturn(0);
        dest.add(t);
    }
}

void Session::replace(juce::Array<Track*>& src)
{
    tracks.clear();
    for (auto t : src)
      tracks.add(t);
}

//////////////////////////////////////////////////////////////////////
//
// OBSOLETE, WEED AND DELETE
//
//////////////////////////////////////////////////////////////////////

#if 0
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
#endif

/**
 * Kludge for MidiTrackEditor
 *
 * Find or create a definition for a track of this type
 * with a logical index.  Meaning if the index is 2 there need to be
 * three tracks accessible with that index to store configuration.
 * Will go away once MidiTrackEditor can handle dynamic track add/remove
 * rather than being fixed at 8 tracks.
 */
#if 0
Session::Track* Session::ensureTrack(TrackType type, int index)
{
    Track* found = getTrackByType(type, index);
    if (found == nullptr) {
        int count = countTracks(type);
        for (int i = count ; i <= index ; i++) {
            found = new Session::Track();
            found->type = type;
            tracks.add(found);
        }
        // give any new ones unique ids
        assignIds();
    }
    return found;
}
#endif

/**
 * Move the tracks from one session to another.
 * Used by MidiTrackEditor
 */
#if 0
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

    assignIds();

    // this is authoritative over how many tracks there logically are
    // the Track array may be sparse or have extras
    midiTracks = src->midiTracks;
}
#endif

//////////////////////////////////////////////////////////////////////
//
// Global Parameters
//
//////////////////////////////////////////////////////////////////////

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
    id = src->id;
    type = src->type;
    name = src->name;
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

void Session::Track::setInt(juce::String pname, int value)
{
    ValueSet* g = ensureParameters();
    g->setInt(pname, value);
}    

void Session::Track::setBool(juce::String pname, bool value)
{
    ValueSet* g = ensureParameters();
    g->setBool(pname, value);
}    

void Session::Track::setString(juce::String pname, const char* value)
{
    ValueSet* g = ensureParameters();
    g->setString(pname, value);
}    

//////////////////////////////////////////////////////////////////////
//
// XML
//
//////////////////////////////////////////////////////////////////////

void Session::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    name = root->getStringAttribute("name");
    location = root->getStringAttribute("location");

    // These are temporary and only import during the early transition
    // from Setups to Sessions. Once that is complete these are not public
    // and can be removed.
    audioTracks = root->getIntAttribute("audioTracks");
    midiTracks = root->getIntAttribute("midiTracks");
        
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName(ValueSet::XmlElement)) {
            ValueSet* set = new ValueSet();
            set->parse(el);
            globals.reset(set);
        }
        else if (el->hasTagName("Track")) {
            tracks.add(parseTrack(el, errors));
        }
        else {
            errors.add(juce::String("Session: Invalid XML element: ") + el->getTagName());
        }
    }

    assignIds();
}

Session::Track* Session::parseTrack(juce::XmlElement* root, juce::StringArray& errors)
{
    Session::Track* track = new Session::Track();

    track->id = root->getIntAttribute("id");
    track->name = root->getStringAttribute("name");
    
    juce::String typeString = root->getStringAttribute("type");
    if (typeString == juce::String("audio")) {
        track->type = Session::TypeAudio;
    }
    else if (typeString == juce::String("midi")) {
        track->type = Session::TypeMidi;
    }
    else {
        errors.add(juce::String("Session: Invalid track type: ") + typeString);
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
            errors.add(juce::String("Session: Invalid XML element: ") + el->getTagName());
        }
    }
    return track;
}

juce::String Session::toXml()
{
    juce::XmlElement root ("Session");

    if (name.length() > 0) root.setAttribute("name", name);
    if (location.length() > 0) root.setAttribute("location", location);

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

    // this needs to be saved and restored for cloning at runtime
    // but it is meaningless when saved in a file
    if (track->id > 0)
      root->setAttribute("id", track->id);

    if (track->type == Session::TypeAudio)
      root->setAttribute("type", "audio");
    else if (track->type == Session::TypeMidi)
      root->setAttribute("type", "midi");
    else
      root->setAttribute("type", "???");

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
