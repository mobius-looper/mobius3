/**
 * Evolving utility to read old mobius.xml and ui.xml files and convert
 * them to the new format.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"

#include "../model/XmlRenderer.h"
#include "../model/MobiusConfig.h"
#include "../model/UIConfig.h"
#include "../model/Preset.h"
#include "../model/Setup.h"
#include "../model/Binding.h"
#include "../model/ScriptConfig.h"

#include "../ui/JuceUtil.h"
#include "../Supervisor.h"

#include "UpgradePanel.h"

const int UpgradePanelHeaderHeight = 20;
const int UpgradePanelFooterHeight = 20;

UpgradePanel::UpgradePanel()
{
    addAndMakeVisible(commands);
    commands.setListener(this);
    commands.add(&loadButton);
    commands.add(&installButton);
    commands.add(&undoButton);
    
    addAndMakeVisible(footer);
    footer.addAndMakeVisible(okButton);
    okButton.addListener(this);
    
    addAndMakeVisible(&log);

    // set this true to enable stricter error checking and filtering
    // need a checkbox...
    strict = false;
    
    setSize(800, 600);
}

UpgradePanel::~UpgradePanel()
{
    // should be using a smart pointer
    delete mobiusConfig;
}

void UpgradePanel::show()
{
    JuceUtil::centerInParent(this);
    setVisible(true);
    log.add("Click \"Load File\" to analyze a configuration file");
    log.add("Click \"Install\" to install a configuration after analysis");
    log.add("Click \"Undo\" to undo the last install");
    locateExisting();
}

void UpgradePanel::buttonClicked(juce::Button* b)
{
    if (b == &okButton) {
        setVisible(false);
    }
    else if (b == &loadButton) {
        doLoad();
    }
    else if (b == &installButton) {
        doInstall();
    }
    else if (b == &undoButton) {
        doUndo();
    }
}

void UpgradePanel::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    area.removeFromBottom(5);
    area.removeFromTop(5);
    area.removeFromLeft(5);
    area.removeFromRight(5);

    area.removeFromTop(UpgradePanelHeaderHeight);

    juce::Rectangle<int> commandArea = area.removeFromTop(20);
    commands.setBounds(commandArea);
    
    footer.setBounds(area.removeFromBottom(UpgradePanelFooterHeight));
    okButton.setSize(60, UpgradePanelFooterHeight);

    log.setBounds(area);

    JuceUtil::centerInParent(&okButton);
}

void UpgradePanel::paint(juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    g.setColour(juce::Colours::white);
    g.drawRect(getLocalBounds(), 4);

    // easier to just give this a container component...
    int headerLeft = 5;
    int headerTop = 5;
    int headerWidth = getWidth() - 10;
    int headerHeight = UpgradePanelHeaderHeight;
    
    g.setColour(juce::Colours::blue);
    g.fillRect(headerLeft, headerTop, headerWidth, headerHeight);
    juce::Font font (UpgradePanelHeaderHeight * 0.8f);
    g.setColour(juce::Colours::white);
    g.drawText("Configuration File Upgrade", headerLeft, headerTop, headerWidth, headerHeight,
               juce::Justification::centred);
}

//////////////////////////////////////////////////////////////////////
//
// Load
//
//////////////////////////////////////////////////////////////////////

/**
 * Attempt to locate the current configuration file.
 * On Windows this starts in c:/Program Files (x86) but gets
 * redirected to the "virtual store" when you write to it.
 */
void UpgradePanel::locateExisting()
{
    expectedVerified = false;
    
    // todo: conditionalize this for Mac
    juce::File home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    expected = home.getChildFile("AppData/Local/VirtualStore/Program Files (x86)/Mobius 2/mobius.xml");
    if (expected.existsAsFile()) {
        log.add("Located existing file at: " + expected.getFullPathName());
        expectedVerified = true;
    }
    else {
        log.add("Unable to locate existing mobius.xml file");
        log.add("The expected location is: " + expected.getFullPathName());
    }
}

void UpgradePanel::doLoad()
{
    doFileChooser();
}

void UpgradePanel::doFileChooser()
{
    juce::File startPath(Supervisor::Instance->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;
    else if (expectedVerified)
      startPath = expected;
    
    // a form of smart pointer
    chooser = std::make_unique<juce::FileChooser> ("Select an XML file ...",
                                                   startPath,
                                                   "*.xml");

    auto chooserFlags = juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles;
    
    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        // magically get here after the modal dialog closes
        // the array will be empty if Cancel was selected
        juce::Array<juce::File> result = fc.getResults();
        if (result.size() > 0) {
            //Trace(2, "FileChooser chose these files:\n");
            for (auto file : result) {

                juce::String path = file.getFullPathName();
                //Trace(2, "  %s\n", path.toUTF8());

                doLoad(file);

                // remember this directory for the next time
                lastFolder = file.getParentDirectory().getFullPathName();
            }
        }
        
    });
}

void UpgradePanel::doLoad(juce::File file)
{
    log.add("");
    log.add("Loading: " + file.getFullPathName());
    
    juce::String xml = file.loadFileAsString();
    if (xml.contains("MobiusConfig")) {
        loadMobiusConfig(xml);
        // hack, button bindings were once in mobius.xml but were moved to ui.xml
        // there can be a mixture, rather than make them load the ui.xml as a sspearate
        // step, can find it now that we know where mobius.xml was
        juce::File sib = file.getSiblingFile("ui.xml");
        if (sib.existsAsFile()) {
            log.add("Loading: " + sib.getFullPathName());
            xml = sib.loadFileAsString();
            loadUIConfig(xml);
        }
    }
    else if (xml.contains("UIConfig")) {
        loadUIConfig(xml);
    }
    else {
        log.add("Error: File does not contain a MobiusConfig");
    }

    log.add("");
    log.add("Click the \"Install\" button to install into the new configuration files");
}

void UpgradePanel::loadUIConfig(juce::String xml)
{
    int buttonCount = 0;
    int buttonAdded = 0;
    
    juce::XmlDocument doc(xml);
    std::unique_ptr<juce::XmlElement> docel = doc.getDocumentElement();
    if (docel == nullptr) {
        log.add("Error: XML parsing error: " + juce::String(doc.getLastParseError()));
    }
    else if (!docel->hasTagName("UIConfig")) {
        log.add("Error: Unexpected XML tag name: " + juce::String(docel->getTagName()));
    }
    else {
        for (auto* el : docel->getChildIterator()) {
            if (el->hasTagName("Buttons")) {
                juce::XmlElement* child = el->getFirstChildElement();
                while (child != nullptr) {
                    if (child->hasTagName("Button")) {
                        buttonCount++;
                        if (convertButton(child))
                          buttonAdded++;
                    }
                    child = child->getNextElement();
                }
            }
        }
    }

    log.add(juce::String(buttonCount) + " ui.xml buttons loaded, retained " +
            juce::String(buttonAdded));
}

bool UpgradePanel::convertButton(juce::XmlElement* el)
{
    bool converted = false;
    juce::String function = el->getStringAttribute("function");
    if (function.length() > 0) {
        DisplayButton* db = new DisplayButton();
        db->action = function;
        if (!addUpgradeButton(db)) {
            log.add(juce::String("Binding for button ") + function + " already exists");
            delete db;
        }
        else {
            converted = true;
        }
    }
    return converted;
}

void UpgradePanel::loadMobiusConfig(juce::String xml)
{
    delete mobiusConfig;
    mobiusConfig = nullptr;
    masterConfig = nullptr;
    
    newPresets.clear();
    newSetups.clear();
    newScripts.clear();
    scriptNames.clear();
    newBindings.clear();
    delete newButtons;
    newButtons = nullptr;
    
    XmlRenderer xr;
    mobiusConfig = xr.parseMobiusConfig(xml.toUTF8());
    if (mobiusConfig == nullptr) {
        log.add("Unable to parse file");
    }
    else {
        log.add("MobiusConfig file parsed");
        bool dumpit = false;
        if (dumpit) {
            char* cxml = xr.render(mobiusConfig);
            log.add(cxml);
            delete cxml;
        }

        masterConfig = Supervisor::Instance->getMobiusConfig();

        loadPresets();
        loadSetups();
        loadScripts();
        loadBindings();
    }
}

void UpgradePanel::loadPresets()
{
    int count = 0;
    
    log.add("Loading Presets...");
    Preset* preset = mobiusConfig->getPresets();
    while (preset != nullptr) {
        count++;
        juce::String name = juce::String(preset->getName());
        log.add("Loading Preset: " + name);

        bool ignore = false;
        juce::String newName;
        Preset* existing = masterConfig->getPreset(preset->getName());
        if (existing != nullptr) {
            newName = "Upgrade:" + juce::String(existing->getName());
            Preset* upgrade = masterConfig->getPreset(newName.toUTF8());
            if (upgrade != nullptr) {
                log.add("Upgraded Preset \"" + newName + "\" already exists");
                log.add("  Delete or rename it if you need to upgrade again");
                ignore = true;
            }
            else {
                log.add("Preset \"" + juce::String(existing->getName()) +
                        "\" already exists, renaming to \"" + newName + "\"");
            }
        }

        if (!ignore) {
            Preset* copy = new Preset(preset);
            if (newName.length() > 0)
              copy->setName(newName.toUTF8());
            newPresets.add(copy);
            log.add("Added Preset: " + juce::String(copy->getName()));
        }
        
        preset = preset->getNextPreset();
    }

    log.add("Loaded " + juce::String(count) + " presets, retained " +
            juce::String(newPresets.size()));
}

void UpgradePanel::loadSetups()
{
    int count = 0;
    
    log.add("Loading Setups...");
    Setup* setup = mobiusConfig->getSetups();
    while (setup != nullptr) {
        count++;
        juce::String name = juce::String(setup->getName());
        log.add("Loading Setup: " + name);

        bool ignore = false;
        juce::String newName;
        Setup* existing = masterConfig->getSetup(setup->getName());
        if (existing != nullptr) {
            newName = "Upgrade:" + juce::String(existing->getName());
            Setup* upgrade = masterConfig->getSetup(newName.toUTF8());
            if (upgrade != nullptr) {
                log.add("Upgraded Setup \"" + newName + "\" already exists");
                log.add("  Delete or rename it if you need to upgrade again");
                ignore = true;
            }
            else {
                log.add("Setup \"" + juce::String(existing->getName()) +
                        "\" already exists, renaming to \"" + newName + "\"");
            }
        }

        if (!ignore) {
            Setup* copy = new Setup(setup);
            if (newName.length() > 0)
              copy->setName(newName.toUTF8());
            newSetups.add(copy);
            log.add("Added Setup: " + juce::String(copy->getName()));
        }
        
        setup = setup->getNextSetup();
    }

    log.add("Loaded " + juce::String(count) + " setups, retained " +
            juce::String(newSetups.size()));
}

void UpgradePanel::loadScripts()
{
    int count = 0;
    
    log.add("Loading Scripts...");
    ScriptConfig* masterScripts = masterConfig->getScriptConfig();
    ScriptConfig* srcScripts = mobiusConfig->getScriptConfig();
    if (srcScripts != nullptr) {
        ScriptRef* ref = srcScripts->getScripts();
        while (ref != nullptr) {
            count++;
            juce::File file (ref->getFile());
            if (file.isDirectory()) {
                log.add("Verified script directory: " + juce::String(ref->getFile()));
                // we keep the reference, but need to descend into it to
                // register what it contains for binding resolution
                ScriptRef* existing = masterScripts->get(ref->getFile());
                if (existing == nullptr) {
                    newScripts.add(new ScriptRef(ref));
                }
                else {
                    log.add("Script path already exists: " + juce::String(ref->getFile()));
                }

                registerDirectoryScripts(file);
            }
            else {
                // this does two things, it verifies that the file exists
                // and if it does, reads enough of it to dig out the !name to add it
                // to the name list
                juce::String scriptName = verifyScript(ref);
                if (scriptName.length() > 0) {
                    // but only add it to the pending list if it isn't already there
                    ScriptRef* existing = masterScripts->get(ref->getFile());
                    if (existing == nullptr) {
                        newScripts.add(new ScriptRef(ref));
                    }
                    else {
                        log.add("Script path already exists: " + juce::String(ref->getFile()));
                    }
                }
                else {
                    // will have already logged a warning
                }
            }
            ref = ref->getNext();
        }
    }

    log.add("Loaded " + juce::String(count) + " scripts, retained " +
            juce::String(newScripts.size()));
}

juce::String UpgradePanel::verifyScript(ScriptRef* ref)
{
    juce::String scriptName;

    juce::File file (ref->getFile());
    if (!file.existsAsFile()) {
        // two options here, ignore it, or add it and let them fix the path later
        // since we need the file to validate bindings to the !name in the script
        // you'll lose bindings unless the binding name happens to be the same
        // as the leaf file name, which it sometimes is
        scriptName = file.getFileNameWithoutExtension();
        log.add("Warning: Script file not found: " + juce::String(ref->getFile()));
        log.add("  Assuming bindings reference the name: " + scriptName);
    }
    else {
        log.add("Verified script file: " + juce::String(ref->getFile()));
        scriptName = getScriptName(file);
    }

    if (scriptName.length() > 0) {
        log.add("Registering script name " + scriptName);
        scriptNames.add(scriptName);
    }
    
    return scriptName;
}

/**
 * When the ScriptConfig contains a directory reference, we just keep the
 * single ScriptRef, but for binding verification we need to descend into it
 * and load all the script names it contains.
 * Hmm, this may be more anal than we need, could just leave unresolved bindings behind
 * and let them sort it out later.
 */
void UpgradePanel::registerDirectoryScripts(juce::File dir)
{
    int types = juce::File::TypesOfFileToFind::findFiles;
    juce::String pattern ("*.mos");
    juce::Array<juce::File> files =
        dir.findChildFiles(types,
                           // searchRecursively   
                           false,
                           pattern,
                           // followSymlinks
                           juce::File::FollowSymlinks::no);
    for (auto file : files) {
        // hmm, I had a case where a renamed .mos file left
        // an emacs save file with the .mos~ extension and this
        // passed the *.mos filter, seems like a bug
        juce::String path = file.getFullPathName();
        if (path.endsWithIgnoreCase(".mos")) {
            juce::String scriptName = getScriptName(file);
            if (scriptName.length() > 0) {
                log.add("Registering script name " + scriptName);
                scriptNames.add(scriptName);
            }
        }
    }
}

juce::String UpgradePanel::getScriptName(juce::File file)
{
    juce::String scriptName;
    
    juce::String text = file.loadFileAsString();
    int nameline = text.indexOf("!name");
    if (nameline < 0) {
        scriptName = file.getFileNameWithoutExtension();
    }
    else {
        int newline = text.indexOf(nameline, "\n");
        if (newline > 0) {
            scriptName = text.substring(nameline + 5, newline).trim();
        }
        else {
            log.add("Warning: Malformed !name statement in script file, ignoring");
        }
    }
    return scriptName;
}

/**
 * When strict mode is on, bindings will be filtered unless they resolve
 * to an existing Symbol, or are found in the registered scripts.
 */
void UpgradePanel::loadBindings()
{
    int setCount = 0;
    int midiCount = 0;
    int keyCount = 0;
    int hostCount = 0;
    int buttonCount = 0;
            
    int midiAdded = 0;
    int hostAdded = 0;
    int buttonAdded = 0;
    
    BindingSet* masterBindings = masterConfig->getBindingSets();
    BindingSet* bindings = mobiusConfig->getBindingSets();

    while (bindings != nullptr) {
        setCount++;
        const char* name = bindings->getName();
        // these normally do not have names
        if (name == nullptr)
          name = "unnamed";
        juce::String jname = juce::String(name);
        log.add("Loading binding set: " + jname);
            
        Binding* binding = bindings->getBindings();
        while (binding != nullptr) {
            Binding* copy = nullptr;
            
            if (binding->trigger == TriggerMidi ||
                binding->isMidi()) {
                midiCount++;

                copy = verifyBinding(binding);
                if (copy != nullptr) {
                    midiAdded++;
                    // adjust the channel
                    copy->midiChannel = binding->midiChannel + 1;
                }
            }
            else if (binding->trigger == TriggerKey) {
                keyCount++;
            }
            else if (binding->trigger == TriggerHost) {
                hostCount++;
                copy = verifyBinding(binding);
                if (copy != nullptr)
                  hostAdded++;
            }
            else if (binding->trigger == TriggerUI) {
                buttonCount++;
                copy = verifyBinding(binding);
                if (copy != nullptr) {
                    // these aren't Bindings in the new model
                    DisplayButton* db = new DisplayButton();
                    db->action = copy->getSymbolName();
                    db->arguments = juce::String(copy->getArguments());
                    db->scope = juce::String(copy->getScope());
                    delete copy;
                    copy = nullptr;
                    if (!addUpgradeButton(db)) {
                        log.add(juce::String("Binding for button ") + binding->getSymbolName() + " already exists");
                        delete db;
                    }
                    else {
                        buttonAdded++;
                    }
                }
            }

            if (copy != nullptr) {
                // it was valid, but ignore if already there
                Binding* existing = masterBindings->findBinding(copy);
                if (existing == nullptr) {
                    newBindings.add(copy);
                }
                else {
                    delete copy;
                    log.add(juce::String("Binding for ") + binding->getSymbolName() + " already exists");
                    // ugh, we already incremented the counter
                    if (binding->trigger == TriggerHost)
                      hostAdded--;
                    else
                      midiAdded--;
                }
            }
            
            binding = binding->getNext();
        }
        bindings = bindings->getNextBindingSet();
    }

    if (setCount > 1) {
        log.add("Warning: Multiple binding sets encountered");
        log.add("  These were formerly called \"overlay\" bindings and are not currently supported");
        log.add("  All binding sets will be merged together when upgrading");
    }
    if (keyCount > 0) {
        log.add("Warning: Keyboard bindings are not currently upgradable, " + 
                juce::String(keyCount) + " ignored");
    }
    
    log.add(juce::String(midiCount) + " MIDI bindings loaded, " +
            juce::String(midiAdded) + " retained");
    
    log.add(juce::String(hostCount) + " Host Parameter bindings loaded, " +
            juce::String(hostAdded) + " retained");
    
    log.add(juce::String(buttonCount) + " UI Button bindings loaded, " +
            juce::String(buttonAdded) + " retained");
}

/**
 * For buttons, we'll collect them in a new ButtonSet with the name "Upgrade"
 * rather than merge them into an existing object.
 * Still need dup checking within the upgrade set though.
 */
bool UpgradePanel::addUpgradeButton(DisplayButton* db)
{
    bool added = false;

    // find or create the upgrade set
    if (newButtons == nullptr) {
        juce::String upgradeName ("Upgrade");
        UIConfig* uiConfig = Supervisor::Instance->getUIConfig();
        ButtonSet* upgrade = uiConfig->findButtonSet(upgradeName);
        if (upgrade == nullptr) {
            newButtons = new ButtonSet();
            newButtons->name = upgradeName;
        }
        else 
          newButtons = new ButtonSet(upgrade);
    }

    // then add the button if it isn't there
    DisplayButton* existing = newButtons->getButton(db);
    if (existing == nullptr) {
        newButtons->buttons.add(db);
        added = true;
    }
    return added;
}

/**
 * Fix some of the most common binding name changes.
 */
Binding* UpgradePanel::verifyBinding(Binding* src)
{
    Binding* copy = new Binding(src);

    juce::String name (src->getSymbolName());
    Symbol* symbol = Symbols.find(name);
    if (symbol != nullptr) {
        // already know about this one
        if (symbol->coreFunction == nullptr &&
            symbol->coreParameter == nullptr) {
            // what the hell is this?
            // don't think there are any new symbols that match old names
            log.add("Warning: Binding resolved to non-core symbol: " + name);
            log.add("  Ask Jeff what's the deal.");
        }
    }
    else if (name.startsWith("Loop")) {
        // two forms, LoopN and the older LoopTriggerN
        int number = getLastDigit(name);
        if (number < 1 || number > 16) {
            log.add("Warning: Loop selection argument out of range: " + juce::String(number));
            number = 1;
        }
        copy->setSymbolName("SelectLoop");
        copy->setArguments(juce::String(number).toUTF8());
    }
    else if (name.startsWith("Track")) {
        // two forms, TrackN and TrackSelectN
        // there is also TrackReset, TrackCopy, and others but those
        // should have been caught in the symbol lookup above
        int number = getLastDigit(name);
        if (number < 1 || number > 32) {
            log.add("Warning: Track selection argument out of range: " + juce::String(number));
            number = 1;
        }
        copy->setSymbolName("SelectTrack");
        copy->setArguments(juce::String(number).toUTF8());
    }
    else if (name.startsWith("Sample")) {
        // I used these all the time, not sure if anyone else did
        int number = getLastDigit(name);
        if (number < 1 || number > 32) {
            log.add("Warning: Sample selection argument out of range: " + juce::String(number));
            number = 1;
        }
        copy->setSymbolName("SamplePlay");
        copy->setArguments(juce::String(number).toUTF8());
    }
    else if (name == juce::String("Speed")) {
        // see this in older test files
        // hmm, what we're basically doing here is implementing old aliases
        // not putting aliases in the symbol table right now, but if there
        // end up being a lot of these consider that
        copy->setSymbolName("SpeedToggle");
    }
    else if (scriptNames.contains(name)) {
        // reference to a script, assuming we could figure out the reference name
    }
    else {
        log.add("Warning: Unresolved binding name \"" + name + "\"");
        if (strict) {
            log.add("  Strict mode active, ignoring binding");
            delete copy;
            copy = nullptr;
        }
    }
    
    return copy;
}

int UpgradePanel::getLastDigit(juce::String s)
{
    juce::String suffix = s.getLastCharacters(1);
    return suffix.getIntValue();
}

//////////////////////////////////////////////////////////////////////
//
// Install
//
//////////////////////////////////////////////////////////////////////

void UpgradePanel::noLoad()
{
    log.add("No configuration file has been loaded");
    log.add("Press the \"Load\" button and select a file");
    log.add("");
    log.add("On Windows, the file location is usually:");
    log.add("  c:\\Program Files (x86)\\Mobius 2\\mobius.xml");
    log.add("");
    log.add("On Mac, the file location is usually:");
    log.add("  /Library/Application Support/Mobius 2/mobius.xml");
}

/**
 * For bindingsets we're mostly interested in MIDI and key bindigs.
 * Host parameters could be useful but less often used.
 * 
 * Could convert button bindings I suppose but they go in UIConfig now
 * with a much more complex model.
 *
 * The most imporant upgrade is for MIDI bindings which changes the channel
 * numbers from 0 based to 1 based.
 *
 * Keyboard bindings are harder as the key codes are different.  The old key codes
 * are in a header file somewhere, could upgrade those with some effort but I
 * don't think many people besides myself use key bindings.
 *
 */
void UpgradePanel::doInstall()
{
    if (mobiusConfig == nullptr) {
        noLoad();
    }
    else {
        log.add("");
                
        masterConfig = Supervisor::Instance->getMobiusConfig();

        // quick and dirty undo
        juce::File root = Supervisor::Instance->getRoot();
        juce::File undo = root.getChildFile("mobius.xml.undo");
        XmlRenderer xr;
        char* cxml = xr.render(masterConfig);
        undo.replaceWithText(juce::String(cxml));
        delete cxml;

        juce::File uiundo = root.getChildFile("uiconfig.xml.undo");
        UIConfig* uiconfig = Supervisor::Instance->getUIConfig();
        juce::String xml = uiconfig->toXml();
        uiundo.replaceWithText(xml);
        
        while (newPresets.size() > 0) {
            Preset* p = newPresets.removeAndReturn(0);
            masterConfig->addPreset(p);
        }
        
        while (newSetups.size() > 0) {
            Setup* s = newSetups.removeAndReturn(0);
            masterConfig->addSetup(s);
        }

        ScriptConfig* masterScripts = masterConfig->getScriptConfig();
        ScriptConfig* srcScripts = mobiusConfig->getScriptConfig();
        if (srcScripts != nullptr) {
            if (masterScripts == nullptr) {
                masterScripts = new ScriptConfig();
                masterConfig->setScriptConfig(masterScripts);
            }

            while (newScripts.size() > 0) {
                ScriptRef* ref = newScripts.removeAndReturn(0);
                masterScripts->add(ref);
            }
        }
        
        BindingSet* masterBindings = masterConfig->getBindingSets();
        while (newBindings.size() > 0) {
            Binding* b = newBindings.removeAndReturn(0);
            masterBindings->addBinding(b);
        }

        if (newButtons != nullptr) {
            UIConfig* config = Supervisor::Instance->getUIConfig();
            ButtonSet* existing = config->findButtonSet(newButtons->name);
            if (existing != nullptr)
              config->buttonSets.removeObject(existing);
            config->buttonSets.add(newButtons);
            newButtons = nullptr;
            Supervisor::Instance->updateUIConfig();
        }

        Supervisor::Instance->updateMobiusConfig();
        
        log.add("MobiusConfig ugprade installed");
        log.add("You may revert these changes by returning to this panel and clicking \"Undo\"");
    }
}

//////////////////////////////////////////////////////////////////////
//
// Undo
//
//////////////////////////////////////////////////////////////////////

void UpgradePanel::doUndo()
{
    log.add("");
    juce::File root = Supervisor::Instance->getRoot();
    
    juce::File undo = root.getChildFile("mobius.xml.undo");
    if (!undo.existsAsFile()) {
        log.add("mobius.xml undo file not found");
    }
    else {
        // rather than work with the memory structures as we normally do,
        // here we'll just slam in new file contents and cause Supervisor to reload it
        juce::String xml = undo.loadFileAsString();
        if (!xml.contains("MobiusConfig")) {
            log.add("ERROR: Undo file does not contain a MobiusConfig");
        }
        else {
            juce::File dest = root.getChildFile("mobius.xml");
            if (!dest.existsAsFile()) {
                log.add("ERROR: Unable to locate the master mobius.xml file");
            }
            else {
                dest.replaceWithText(xml);
                Supervisor::Instance->reloadMobiusConfig();
                log.add("The previous mobius.xml installation has been undone");

                // hmm, should we let this linger?
                // if they make manual changes and decide they don't want them
                // might be useful
                // for that matter, a general undo for all config edits would be nice
                //undo.deleteFile();
            }
        }
    }
    
    undo = root.getChildFile("uiconfig.xml.undo");
    if (!undo.existsAsFile()) {
        log.add("uiconfig.xml undo file not found");
    }
    else {
        juce::String xml = undo.loadFileAsString();
        if (!xml.contains("UIConfig")) {
            log.add("ERROR: Undo file does not contain a UIConfig");
        }
        else {
            juce::File dest = root.getChildFile("uiconfig.xml");
            if (!dest.existsAsFile()) {
                log.add("ERROR: Unable to locate the master uiconfig.xml file");
            }
            else {
                dest.replaceWithText(xml);
                Supervisor::Instance->reloadUIConfig();
                log.add("The previous uiconfig.xml installation has been undone");
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
