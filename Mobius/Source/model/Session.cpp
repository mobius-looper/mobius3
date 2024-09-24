
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
    for (auto track : src->tracks) {
        tracks.add(new Track(track));
    }

    if (src->globals != nullptr)
      globals.reset(new ValueSet(src->globals.get()));
}

int Session::getAudioTrackCount()
{
    return audioTracks;
}

int Session::getMidiTrackCount()
{
    int count = 0;
    for (auto track : tracks) {
        if (track->type == TypeMidi)
          count++;
    }
    return count;
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
    if (src->parameters != nullptr)
      parameters.reset(new ValueSet(src->parameters.get()));
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
            ValueSet* set = new ValueSet();
            set->parse(el);
            track->parameters.reset(set);
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
    

    root->setAttribute("name", track->name);
    
    if (track->parameters != nullptr)
      track->parameters->render(root);
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
