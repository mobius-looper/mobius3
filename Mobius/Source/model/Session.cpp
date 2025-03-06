
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "ValueSet.h"
#include "Symbol.h"
#include "ParameterProperties.h"
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
        Track* copy = new Track(track);
        add(copy);
    }

    if (src->globals != nullptr)
      globals.reset(new ValueSet(src->globals.get()));

    // source tracks should already have ids but make sure
    assignIds();

    // there is only one of these so it's save to copy
    symbols = src->getSymbols();
}

void Session::setSymbols(SymbolTable* st)
{
    symbols = st;
}

SymbolTable* Session::getSymbols()
{
    return symbols;
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

/**
 * When a Session is created from scratch, set default values.
 */


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
            add(neu);
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
    if (t != nullptr) {
        // this should be the only way to add a track to ensure
        // the back pointer is set
        t->setSession(this);
        tracks.add(t);
    }
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
        // this will no longer be valid
        t->setSession(nullptr);
        dest.add(t);
    }
}

void Session::replace(juce::Array<Track*>& src)
{
    tracks.clear();
    for (auto t : src)
      add(t);
}

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

MslValue* Session::get(SymbolId sid)
{
    MslValue* v = nullptr;
    if (symbols == nullptr) {
        Trace(1, "Session: No symbol table for reference resolution");
    }
    else {
        Symbol* s = symbols->getSymbol(sid);
        if (s == nullptr)
          Trace(1, "Session: No symbol for id, can't happen in my backyard");
        else
          v = get(s);
    }
    return v;
}

MslValue* Session::get(Symbol* s)
{
    MslValue* v = nullptr;
    if (s == nullptr)
      Trace(1, "Session: No symbol");
    else {
        v = get(s->name);

        // hack for newly created empty sessions
        // if there is no explicit value, and the parameter defined a default
        // bootstrap a value
        if (v == nullptr && s->parameterProperties != nullptr) {
            int dflt = s->parameterProperties->defaultValue;
            if (dflt > 0) {
                (void)ensureGlobals();
                MslValue dv;
                dv.setInt(dflt);
                globals->set(s->name, dv);
                v = globals->get(s->name);
            }
        }
    }
    return v;
}

/**
 * Used in a few cases where we put transient things in the session
 * for editing then need to move them somewhere else.
 * e.g. plugin io pins
 */
void Session::remove(juce::String pname)
{
    if (globals != nullptr)
      globals->remove(pname);
}

bool Session::getBool(juce::String pname)
{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getBool() : false;
}

bool Session::getBool(SymbolId sid)
{
    MslValue* v = get(sid);
    return (v != nullptr) ? v->getBool() : false;
}

int Session::getInt(juce::String pname)

{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getInt() : false;
}

int Session::getInt(SymbolId sid)
{
    MslValue* v = get(sid);
    return (v != nullptr) ? v->getInt() : false;
}

const char* Session::getString(juce::String pname)
{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getString() : nullptr;
}

const char* Session::getString(SymbolId sid)
{
    MslValue* v = get(sid);
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

/**
 * Assimilate a ValueSet into the global set.
 * Used during upgrade of old Presets.
 * This is somewhat dangerous if you're not careful where the set came from.
 */
void Session::assimilate(ValueSet* src)
{
    ValueSet* g = ensureGlobals();
    g->assimilate(src);
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

void Session::Track::setSession(Session* s)
{
    session = s;
}

Session* Session::Track::getSession()
{
    return session;
}

ValueSet* Session::Track::getParameters()
{
    return parameters.get();
}

ValueSet* Session::Track::stealParameters()
{
    return parameters.release();
}

void Session::Track::replaceParameters(ValueSet* neu)
{
    parameters.reset(neu);
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

MslValue* Session::Track::get(SymbolId sid)
{
    MslValue* v = nullptr;
    if (session == nullptr)
      Trace(1, "Session::Track Unable to resolve symbol id, no session pointer");
    else {
        // I'm forgetting how inner classes work
        // "symbols" seems to be in scope here from the Session,
        // verify this
        // SymbolTable* symbols = session->getSymbols();
        SymbolTable* table = session->getSymbols();
        if (table == nullptr)
          Trace(1, "Session::Track Unable to resolve symbol id, no symbol table");
        else {
            Symbol* s = table->getSymbol(sid);
            if (s == nullptr)
              Trace(1, "Session::Track Unable to resolve symbol id");
            else {
                v = get(s);
            }
        }
    }
    return v;
}

MslValue* Session::Track::get(Symbol* s)
{
    MslValue* v = nullptr;
    if (s == nullptr)
      Trace(1, "Session::Track No symbol");
    else {
        v = get(s->name);

        // hack for newly created empty sessions
        // if there is no explicit value, and the parameter defined a default
        // bootstrap a value
        if (v == nullptr && s->parameterProperties != nullptr) {
            int dflt = s->parameterProperties->defaultValue;
            if (dflt > 0) {
                ValueSet* params = ensureParameters();
                MslValue dv;
                dv.setInt(dflt);
                params->set(s->name, dv);
                v = params->get(s->name);
            }
        }
    }
    return v;
}

bool Session::Track::getBool(juce::String pname)
{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getBool() : false;
}

bool Session::Track::getBool(SymbolId sid)
{
    MslValue* v = get(sid);
    return (v != nullptr) ? v->getBool() : false;
}

int Session::Track::getInt(juce::String pname)
{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getInt() : false;
}

int Session::Track::getInt(SymbolId sid)
{
    MslValue* v = get(sid);
    return (v != nullptr) ? v->getInt() : false;
}

const char* Session::Track::getString(juce::String pname)
{
    MslValue* v = get(pname);
    return (v != nullptr) ? v->getString() : nullptr;
}

const char* Session::Track::getString(SymbolId sid)
{
    MslValue* v = get(sid);
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
            Session::Track* t = parseTrack(el, errors);
            add(t);
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
