#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../model/ScriptConfig.h"
#include "../model/ValueSet.h"

#include "MslState.h"
#include "MslBinding.h"
#include "MslValue.h"

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

/**
 * Horrible kludge for fucking Windows path names.
 * For reasons I haven't determined, the full path string on Windows
 * can differ in case in the drive letter, e.g c:\ vs C:\
 *
 * Windows has a case-insensitive file system, but paths tend to preserve
 * case as created by the user, except for the drive letter.  Depending on
 * where the path came from the letter may be upper or lower.  I almost always
 * see lower, perhaps Juce normalizes it, but it is occasionally upper,
 * maybe out of native file chooser dialogs, I'm not sure.
 *
 * Whatever the reason, when looking for files using path strings we
 * must ignore the case of drive letter.  We could also try normalizing
 * it but that's fragile and hard to ensure.
 *
 * I'm also a little leery of doing case insensitive on the entire path
 * but that can only happen on Windows, Darwin is usually case sensitive.
 */
bool ScriptRegistry::Machine::pathEqual(juce::String p1, juce::String p2)
{
    bool equal = (p1 == p2);
    // Mac/Darwin paths can't have colons in them, right?   Buehler?
    if (!equal && p1.indexOf(":") > 0)
      equal = p1.equalsIgnoreCase(p2);

    return equal;
}

ScriptRegistry::File* ScriptRegistry::Machine::findFile(juce::String& path)
{
    File* found = nullptr;
    for (auto file : files) {
        if (pathEqual(file->path, path)) {
            found = file;
            break;
        }
    }
    return found;
}

ScriptRegistry::File* ScriptRegistry::Machine::findFileByName(juce::String& refname)
{
    File* found = nullptr;
    for (auto file : files) {
        if (file->name == refname) {
            found = file;
            break;
        }
    }
    return found;
}

bool ScriptRegistry::Machine::removeFile(juce::String& path)
{
    File* found = findFile(path);
    
    if (found != nullptr)
      files.removeObject(found, true);
    
    return (found != nullptr);
}

ScriptRegistry::External* ScriptRegistry::Machine::findExternal(juce::String& path)
{
    External* found = nullptr;
    for (auto ext : externals) {
        if (pathEqual(ext->path, path)) {
            found = ext;
            break;
        }
    }
    return found;
}

bool ScriptRegistry::Machine::removeExternal(juce::String& path)
{
    External* found = findExternal(path);
    
    if (found != nullptr)
      externals.removeObject(found, true);
    
    return (found != nullptr);
}

void ScriptRegistry::Machine::removeExternal(External* ext)
{
    externals.removeObject(ext, true);
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
/*
juce::OwnedArray<ScriptRegistry::File>* ScriptRegistry::getFiles()
{
    Machine* machine = getMachine();
    return &(machine->files);
}
*/

juce::StringArray ScriptRegistry::Machine::getExternalPaths()
{
    juce::StringArray paths;
    for (auto ext : externals)
      paths.add(ext->path);
    return paths;
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
            External* ext = machine->findExternal(path);
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
            else if (el->hasTagName("MslState")) {
                state.reset(parseState(el));
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

                    // this is where we effectively delete files from the editor
                    if (!file->deleted) {
                    
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
    }

    if (state != nullptr)
      toXml(&root, state.get());
    
    return root.toString();
}

/**
 * MslState doesn't have an XML renderer, it's simple enough but could have
 * it define it's own string representation for these.
 */
void ScriptRegistry::toXml(juce::XmlElement* root, MslState* stateobj)
{
    juce::XmlElement* elState = new juce::XmlElement("MslState");
    root->addChildElement(elState);
    for (auto unit : stateobj->units) {
        juce::XmlElement* elUnit = new juce::XmlElement("Unit");
        elState->addChildElement(elUnit);
        elUnit->setAttribute("id", unit->id);

        for (auto var : unit->variables) {
            juce::XmlElement* elVar = new juce::XmlElement("Variable");
            elUnit->addChildElement(elVar);

            elVar->setAttribute("name", var->name);
            if (var->scopeId > 0)
              elVar->setAttribute("scopeId", var->scopeId);

            // this part is duplicated with ValueSet unfortunately
            if (!var->value.isNull()) {
                
                elVar->setAttribute("value", var->value.getString());

                // only expecting a few types and NO lists yet
                if (var->value.type == MslValue::Int) {
                    elVar->setAttribute("type", juce::String("int"));
                }
                else if (var->value.type == MslValue::Bool) {
                    elVar->setAttribute("type", juce::String("bool"));
                }
                else if (var->value.type == MslValue::Enum) {
                    elVar->setAttribute("type", juce::String("enum"));
                    elVar->setAttribute("ordinal", juce::String(var->value.getInt()));
                }
                else if (var->value.type != MslValue::String) {
                    // float, list, Symbol
                    // shouldn't see these in a value set yet
                    Trace(1, "ScriptRegistry: Incompelte serialization of type %d",
                          (int)var->value.type);
                }
            }
        }
    }
}

MslState* ScriptRegistry::parseState(juce::XmlElement* root)
{
    MslState* stateobj = new MslState();
    
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Unit")) {
            MslState::Unit* unit = new MslState::Unit();
            stateobj->units.add(unit);
            unit->id = el->getStringAttribute("id");
            for (auto* uel : el->getChildIterator()) {
                if (uel->hasTagName("Variable")) {
                    MslState::Variable* var = new MslState::Variable();
                    unit->variables.add(var);
                    var->name = uel->getStringAttribute("name");
                    var->scopeId = uel->getIntAttribute("scopeId");

                    juce::String type = uel->getStringAttribute("type");

                    if (type.length() == 0) {
                        // string
                        juce::String jstr = uel->getStringAttribute("value");
                        var->value.setString(jstr.toUTF8());
                    }
                    else if (type == "int") {
                        var->value.setInt(uel->getIntAttribute("value"));
                    }
                    else if(type == "bool") {
                        // uel->getBoolAttribute should have the same rules as MslValue
                        // make sure of this! basically "true" and not "true"
                        var->value.setInt(uel->getBoolAttribute("value"));
                    }
                    else if (type == "enum") {
                        // the weird one
                        juce::String jstr = uel->getStringAttribute("value");
                        int ordinal = uel->getIntAttribute("ordinal");
                        var->value.setEnum(jstr.toUTF8(), ordinal);
                    }
                    else {
                        // leave null
                        Trace(1, "ScriptRegistry: Invalid value type %s", type.toUTF8());
                    }
                }
            }
        }
    }
    return stateobj;
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
