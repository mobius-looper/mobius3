#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/ScriptConfig.h"

#include "ScriptRegistry.h"

bool ScriptRegistry::convert(ScriptConfig* config)
{
    bool changed = false;
    
    if (config != nullptr) {
        for (ScriptRef* ref = config->getScripts() ; ref != nullptr ; ref = ref->getNext()) {
            juce::String path = juce::String(ref->getFile());
            External* ext = findExternal(path);
            if (ext == nullptr) {
                ext = new External();
                ext->path = path;
                externals.add(ext);
                changed = true;
            }
        }
    }
    return changed;
}

ScriptRegistry::External* ScriptRegistry::findExternal(juce::String& path)
{
    External* found = nullptr;
    for (auto ext : externals) {
        if (ext->path == path) {
            found = ext;
            break;
        }
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// XML
//
//////////////////////////////////////////////////////////////////////

void ScriptRegistry::parseXml(juce::String xml)
{
    juce::XmlDocument doc(xml);
    std::unique_ptr<juce::XmlElement> root = doc.getDocumentElement();
    if (root == nullptr) {
        xmlError("Parse parse error: %s\n", doc.getLastParseError());
    }
    else if (!root->hasTagName("ScriptRegistry")) {
        xmlError("Unexpected XML tag name: %s\n", root->getTagName());
    }
    else {
        // todo: an attribute that redirects the master library folder?
        // no, just keep it under the install folder for now
        
        for (auto* el : root->getChildIterator()) {
            if (el->hasTagName("Externals")) {
                for (auto* el2 : el->getChildIterator()) {
                    if (el2->hasTagName("External")) {
                        External* ext = new External();
                        ext->path = el2->getStringAttribute("path");
                        externals.add(ext);
                    }
                    else {
                        xmlError("Unexpected XML tag name: %s\n", el2->getTagName());
                    }
                }
            }
            else if (el->hasTagName("Files")) {
                for (auto* el2 : el->getChildIterator()) {
                    if (el2->hasTagName("File")) {
                        File* s = new File();
                        s->path = el2->getStringAttribute("path");
                        s->name = el2->getStringAttribute("name");
                        s->library = el2->getBoolAttribute("library");
                        s->author = el2->getStringAttribute("author");
                        s->old = el2->getBoolAttribute("old");
                        s->added = parseTime(el2->getStringAttribute("added"));
                        s->button = el2->getBoolAttribute("button");
                        s->disabled = el2->getBoolAttribute("disabled");
                        files.add(s);
                    }
                    else {
                        xmlError("Unexpected XML tag name: %s\n", el2->getTagName());
                    }
                }
            }
            else {
                xmlError("Unexpected XML tag name: %s\n", el->getTagName());
            }
        }
    }
}

void ScriptRegistry::xmlError(const char* msg, juce::String arg)
{
    juce::String fullmsg ("ScriptRegistry: " + juce::String(msg));
    if (arg.length() == 0)
      Trace(1, fullmsg.toUTF8());
    else
      Trace(1, fullmsg.toUTF8(), arg.toUTF8());
}

juce::String ScriptRegistry::toXml()
{
    juce::XmlElement root ("ScriptRegistry");

    if (externals.size() > 0) {
        juce::XmlElement* elExternals = new juce::XmlElement("Externals");
        root.addChildElement(elExternals);

        for (auto external : externals) {
            juce::XmlElement* elExternal = new juce::XmlElement("External");
            elExternals->addChildElement(elExternal);
            
            elExternal->setAttribute("path", external->path);
            // don't need to save missing or folder, those are set when loaded and verified
        }
    }

    if (files.size() > 0) {
        juce::XmlElement* elFiles = new juce::XmlElement("Files");
        root.addChildElement(elFiles);

        for (auto file : files) {
            juce::XmlElement* fe = new juce::XmlElement("File");
            elFiles->addChildElement(fe);
            
            fe->setAttribute("path", file->path);
            fe->setAttribute("name", file->name);
            fe->setAttribute("author", file->name);
            fe->setAttribute("added", renderTime(file->added));
            fe->setAttribute("library", file->library);
            fe->setAttribute("old", file->old);
            fe->setAttribute("button", file->button);
            fe->setAttribute("disabled", file->disabled);
        }
    }

    return root.toString();
}

/**
 * Unclear how we should render dates.
 * Saving just the utime is flexible but looks ugly in the file.
 * We can parse a utime, but I don't see a parser for the printed representation.
 */
juce::String ScriptRegistry::renderTime(juce::Time& t)
{
    return juce::String(t.toMilliseconds());
}

juce::Time ScriptRegistry::parseTime(juce::String src)
{
    return juce::Time(src.getIntValue());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
