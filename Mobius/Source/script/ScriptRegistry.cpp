#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/ScriptConfig.h"

#include "ScriptRegistry.h"

//////////////////////////////////////////////////////////////////////
//
// Machine Management
//
//////////////////////////////////////////////////////////////////////

/**
 * Find or bootstrap a Machine configuration for the local host.
 */
ScriptRegistry::Machine* ScriptRegistry::getMachine()
{
    juce::String name = juce::SystemStats::getComputerName();
    Machine* machine = findMachine(name);

    if (machine == nullptr) {
        Trace(2, "ScriptRegistry: Bootstrapping ScriptRegistry for host %s\n", name.toUTF8());
        machine = new Machine();
        machine->name = name;
        machines.add(machine);
    }
    return machine;
}

ScriptRegistry::Machine* ScriptRegistry::findMachine(juce::String& name)
{
    Machine* found = nullptr;
    for (auto machine : machines) {
        if (machine->name == name) {
            found = machine;
            break;
        }
    }
    return found;
}

ScriptRegistry::File* ScriptRegistry::Machine::findFile(juce::String& path)
{
    File* found = nullptr;
    for (auto file : files) {
        if (file->path == path) {
            found = file;
            break;
        }
    }
    return found;
}

/**
 * Remove external entries if they have a path residing in the
 * specified folder.  Used to take out redundant entries
 * for files that are in the standard library folder.  Common
 * when converting the old ScriptConfig.
 */
void ScriptRegistry::Machine::filterExternals(juce::String infolder)
{
    // really need to find the right way to both iterate and remove
    // that doesn't involve indexes
    juce::Array<External*> redundant;
    for (auto ext : externals) {
        if (ext->path.startsWith(infolder))
          redundant.add(ext);
    }
    for (auto ext : redundant) {
        Trace(2, "ScriptRegistry: Removing redundant external %s", ext->path.toUTF8());
        externals.removeObject(ext, true);
    }
}

juce::OwnedArray<ScriptRegistry::File>* ScriptRegistry::getFiles()
{
    Machine* machine = getMachine();
    return &(machine->files);
}
        
//////////////////////////////////////////////////////////////////////
//
// ScriptConfig Conversion
//
//////////////////////////////////////////////////////////////////////

/**
 * ScriptConfig was not multi-machine so it will be installed
 * into the current machine.
 */
bool ScriptRegistry::convert(ScriptConfig* config)
{
    bool changed = false;

    if (config != nullptr) {
        
        Machine* machine = getMachine();
        
        for (ScriptRef* ref = config->getScripts() ; ref != nullptr ; ref = ref->getNext()) {
            juce::String path = juce::String(ref->getFile());
            External* ext = findExternal(machine, path);
            if (ext == nullptr) {
                ext = new External();
                ext->path = path;
                machine->externals.add(ext);
                changed = true;
            }
        }
    }
    return changed;
}

ScriptRegistry::External* ScriptRegistry::findExternal(Machine* m, juce::String& path)
{
    External* found = nullptr;
    for (auto ext : m->externals) {
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

            if (el->hasTagName("Machine")) {

                Machine* machine = new Machine();
                machine->name = el->getStringAttribute("name");
                machines.add(machine);
                
                for (auto* el2 : el->getChildIterator()) {
                    
                    if (el2->hasTagName("Externals")) {
                        for (auto* el3 : el2->getChildIterator()) {
                            if (el3->hasTagName("External")) {
                                External* ext = new External();
                                ext->path = el3->getStringAttribute("path");
                                machine->externals.add(ext);
                            }
                            else {
                                xmlError("Unexpected XML tag name: %s\n", el3->getTagName());
                            }
                        }
                    }
                    else if (el2->hasTagName("Files")) {
                        for (auto* el3 : el2->getChildIterator()) {
                            if (el3->hasTagName("File")) {
                                File* s = new File();
                                s->path = el3->getStringAttribute("path");
                                s->name = el3->getStringAttribute("name");
                                s->library = el3->getBoolAttribute("library");
                                s->author = el3->getStringAttribute("author");
                                s->added = parseTime(el3->getStringAttribute("added"));
                                s->button = el3->getBoolAttribute("button");
                                s->disabled = el3->getBoolAttribute("disabled");
                                machine->files.add(s);
                            }
                            else {
                                xmlError("Unexpected XML tag name: %s\n", el3->getTagName());
                            }
                        }
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

    if (machines.size() > 0) {
        for (auto machine : machines) {
        
            juce::XmlElement* elMachine = new juce::XmlElement("Machine");
            root.addChildElement(elMachine);
            elMachine->setAttribute("name", machine->name);

            if (machine->externals.size() > 0) {
                juce::XmlElement* elExternals = new juce::XmlElement("Externals");
                elMachine->addChildElement(elExternals);

                for (auto external : machine->externals) {
                    juce::XmlElement* elExternal = new juce::XmlElement("External");
                    elExternals->addChildElement(elExternal);
            
                    elExternal->setAttribute("path", external->path);
                    // don't need to save missing or folder, those are set when loaded and verified
                }
            }

            if (machine->files.size() > 0) {
                juce::XmlElement* elFiles = new juce::XmlElement("Files");
                elMachine->addChildElement(elFiles);

                for (auto file : machine->files) {
                    juce::XmlElement* fe = new juce::XmlElement("File");
                    elFiles->addChildElement(fe);

                    fe->setAttribute("path", file->path);

                    if (file->name.length() > 0)
                      fe->setAttribute("name", file->name);
                    
                    if (file->author.length() > 0)
                      fe->setAttribute("author", file->author);
                    
                    fe->setAttribute("added", renderTime(file->added));

                    if (file->library)
                      fe->setAttribute("library", file->library);

                    if (file->button)
                      fe->setAttribute("button", file->button);

                    if (file->disabled)
                      fe->setAttribute("disabled", file->disabled);
                }
            }
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

//////////////////////////////////////////////////////////////////////
//
// File
//
//////////////////////////////////////////////////////////////////////

/**
 * The ScriptEditor needs a private copy of the File that won't be
 * deleted out from under it if the registry is refreshed and and
 * a native file was deleted.
 *
 * The MslDetails is the hard part.
 *
 * The External reference is a problem since those can be deleted too.
 * Do not copy that.
 *
 * Much if this isn't necessary but be thorough.
 *
 * Might be nice if we treated File as an interned thing that could live
 * forever.  Hmm, liking that more and more.  Just mark it "missing" rather
 * than deleting it.
 */
ScriptRegistry::File* ScriptRegistry::File::cloneForEditor()
{
    File* clone = new ScriptRegistry::File();
    clone->path = path;
    clone->added = added;
    clone->name = name;
    clone->library = library;
    clone->author = author;
    clone->button = button;
    clone->disabled = disabled;
    clone->missing = missing;
    clone->old = old;

    // skip external

    // unit lives long and propspers
    if (unit != nullptr)
      clone->unit.reset(cloneDetails(unit.get()));

    return clone;
}

MslDetails* ScriptRegistry::File::cloneDetails(MslDetails* src)
{
    // ugh, this is huge and complicated
    Trace(1, "ScriptRegistry::File::cloneDetails not implmeented");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
