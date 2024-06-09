/**
 * Utility to read old mobius.xml and ui.xml files and convert them
 * to the new model.
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Util.h"

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
    commands.add(&loadCurrentButton);
    commands.add(&loadFileButton);
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
    delete newButtons;
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

/**
 * Attempt to locate the current configuration file.
 * On Windows this starts in c:/Program Files (x86) but gets
 * redirected to the "virtual store" when you write to it.
 */
void UpgradePanel::locateExisting()
{
    expectedVerified = false;
    
#ifdef __APPLE__
    // OG Mobius put things under /Library which is normally a shared location
    // and we had to hack around file permissions so we could write there
    // commonApplicationDataDirectory is normally /Library
    juce::File appdata = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);
    // this is where it should have gone, /Users/<user>/Library
    //juce::File appdata = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    expected = appdata.getChildFile("Application Support/Mobius 2/mobius.xml");
#else
    // OG Mobius installed everything in c:/Program Files, but when you write
    // to that location Windows quetly redirects it to this magic location
    // since users don't normally have write access to Program Files
    // userHomeDirectory is normally c:/Users/<name>/
    juce::File home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    expected = home.getChildFile("AppData/Local/VirtualStore/Program Files (x86)/Mobius 2/mobius.xml");
#endif
    
    if (expected.existsAsFile()) {
        log.add("Located existing file at: " + expected.getFullPathName());
        expectedVerified = true;
    }
    else {
        log.add("Unable to locate existing mobius.xml file");
        log.add("The expected location is: " + expected.getFullPathName());
    }
}

void UpgradePanel::buttonClicked(juce::Button* b)
{
    if (b == &okButton) {
        setVisible(false);
    }
    else if (b == &loadCurrentButton) {
        doLoadCurrent();
    }
    else if (b == &loadFileButton) {
        doLoadFile();
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

void UpgradePanel::doLoadCurrent()
{
    if (expectedVerified)
      doLoad(expected);
    else {
        log.add("");
        log.add("Expected installation file not found: " + expected.getFullPathName());
        log.add("Click the \"Load File\" button to search for one");
    }
}

void UpgradePanel::doLoadFile()
{
    doFileChooser();
}

void UpgradePanel::doFileChooser()
{
    // start in the located file if we could find one, otherwise installation root
    juce::File startPath(Supervisor::Instance->getRoot());
    if (lastFolder.length() > 0)
      startPath = lastFolder;

    // now that we have different LoadCurrent and LoadFile buttons
    // start in the dev directory which is more convenient for testing
    //else if (expectedVerified)
    //startPath = expected;
    
    // unlike most Jucey things, we have to delete this 
    chooser = std::make_unique<juce::FileChooser> ("Select the mobius.xml file ...",
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

/**
 * Here from the file chooser.  Normally file will be mobius.xml
 * but allow it to be ui.xml as well for testing.
 */
void UpgradePanel::doLoad(juce::File file)
{
    log.add("");
    log.add("Loading: " + file.getFullPathName());
    
    juce::String xml = file.loadFileAsString();
    if (xml.contains("MobiusConfig")) {
        
        loadMobiusConfig(xml);
        // auto-load the ui.xml file as well which is normally right next to it
        // button bindings were once in mobius.xml but were moved to ui.xml
        // there can be a mixture, rather than make them load the ui.xml as a sspearate
        // step, can find it now that we know where mobius.xml was
        juce::File sib = file.getSiblingFile("ui.xml");
        if (sib.existsAsFile()) {
            log.add("");
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

/**
 * Load the old UIConfig and convert <Button> elements into
 * DisplayButtons.  To support older bindings which were stored
 * in a mobius.xml BindingConfig, we look in both places and
 * merge them.
 */
void UpgradePanel::loadUIConfig(juce::String xml)
{
    int buttonCount = 0;
    int buttonAdded = 0;
    
    juce::XmlDocument doc(xml);
    std::unique_ptr<juce::XmlElement> docel = doc.getDocumentElement();
    if (docel == nullptr) {
        log.add("Error: XML parse error: " + juce::String(doc.getLastParseError()));
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

    log.add(juce::String(buttonCount) + " ui.xml buttons loaded, new " +
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
 * MobiusConfig loading creates new object lists containing
 * only things that are not already in the new model.
 */
void UpgradePanel::loadMobiusConfig(juce::String xml)
{
    delete mobiusConfig;
    mobiusConfig = nullptr;
    masterConfig = nullptr;
    
    newPresets.clear();
    newSetups.clear();
    newScripts.clear();
    scriptNames.clear();
    newBindingSets.clear();
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

/**
 * Presets and Setups are relatively easy.
 * If one already exists with the name "Default"
 * the imported one is given the prefix "Upgrade:" so it doesn't
 * conflict with the default preset, just case they need to
 * get back to something stable.
 *
 * For others, if one exists, it has probably been imported already
 * and we don't need to do it again.  In rare cases, they could
 * have made changes to the imported XML files and want those, they'll
 * have to delete the existing ones first.
 */
void UpgradePanel::loadPresets()
{
    int count = 0;
    
    log.add("");
    log.add("Loading Presets...");
    Preset* preset = mobiusConfig->getPresets();
    while (preset != nullptr) {
        count++;
        juce::String name = juce::String(preset->getName());
        bool ignore = false;
        bool renamed = false;

        if (name == juce::String("Default")) {
            name = "Upgrade:" + name;
            renamed = true;
        }
        
        Preset* existing = masterConfig->getPreset(name.toUTF8());
        if (existing == nullptr) {
            log.add("Preset: " + name);
        }
        else {
            log.add("Preset: " + name + " already exists");
            ignore = true;
        }

        if (!ignore) {
            Preset* copy = new Preset(preset);
            if (renamed)
              copy->setName(name.toUTF8());
            newPresets.add(copy);
        }
        
        preset = preset->getNextPreset();
    }

    log.add("Loaded " + juce::String(count) + " presets, new " +
            juce::String(newPresets.size()));
}

void UpgradePanel::loadSetups()
{
    int count = 0;
    
    log.add("");
    log.add("Loading Setups...");
    Setup* setup = mobiusConfig->getSetups();
    while (setup != nullptr) {
        count++;
        juce::String name = juce::String(setup->getName());
        bool ignore = false;
        bool renamed = false;

        if (name == juce::String("Default")) {
            name = "Upgrade:" + name;
            renamed = true;
        }
        
        Setup* existing = masterConfig->getSetup(name.toUTF8());
        if (existing == nullptr) {
            log.add("Setup: " + name);
        }
        else {
            log.add("Setup: " + name + " already exists");
            ignore = true;
        }

        if (!ignore) {
            Setup* copy = new Setup(setup);
            if (renamed) 
              copy->setName(name.toUTF8());
            newSetups.add(copy);
        }
        
        setup = setup->getNextSetup();
    }

    log.add("Loaded " + juce::String(count) + " setups, new " +
            juce::String(newSetups.size()));
}

/**
 * Loading scripts does two things.  First we accumulate a list
 * of ScriptRef objects for the files and directories containing
 * the scripts.  Then we analyze the contents of the script to determine
 * what the visible binding name for that would have been.  This is used
 * later in verifyBinding to warn about bindings to things that
 * don't exist.
 */
void UpgradePanel::loadScripts()
{
    int count = 0;
    
    log.add("");
    log.add("Loading Scripts...");
    ScriptConfig* masterScripts = masterConfig->getScriptConfig();
    ScriptConfig* srcScripts = mobiusConfig->getScriptConfig();
    if (srcScripts != nullptr) {
        ScriptRef* ref = srcScripts->getScripts();
        while (ref != nullptr) {
            count++;
            bool skipValidation = false;
            // this is a testing convenience for Mac files loaded on Windows
            // avoid a Juce assertion if we know the path isn't going to go anywhere
#ifndef __APPLE__
            const char* path = ref->getFile();
            if (path != nullptr && path[0] == '/')
              skipValidation = true;
#endif
            if (skipValidation) {
                ScriptRef* existing = masterScripts->get(ref->getFile());
                if (existing == nullptr) {
                    log.add("Adding script: " + juce::String(ref->getFile()));
                    newScripts.add(new ScriptRef(ref));
                }
                else {
                    // this doesn't really help much and adds clutter
                    //log.add("Script path already exists: " + juce::String(ref->getFile()));
                }
            }
            else {
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
                        //log.add("Script path already exists: " + juce::String(ref->getFile()));
                    }

                    registerDirectoryScripts(file);
                }
                else {
                    // this verifies that the file exists, and extracts the binding name
                    juce::String scriptName = verifyScript(ref);
                    if (scriptName.length() > 0) {
                        // but only add it to the pending list if it isn't already there
                        ScriptRef* existing = masterScripts->get(ref->getFile());
                        if (existing == nullptr) {
                            newScripts.add(new ScriptRef(ref));
                        }
                        else {
                            // log.add("Script path already exists: " + juce::String(ref->getFile()));
                        }
                    }
                    else {
                        // will have already logged a warning
                    }
                }
            }
            ref = ref->getNext();
        }
    }

    log.add("Registered script names:");
    for (auto name : scriptNames)
      log.add("  " + name);

    log.add("Loaded " + juce::String(count) + " scripts, new " +
            juce::String(newScripts.size()));
}

juce::String UpgradePanel::verifyScript(ScriptRef* ref)
{
    juce::String scriptName;

    juce::File file (ref->getFile());
    if (!file.existsAsFile()) {
        // two options here, ignore it, or add it and let them fix the path later
        // since we need the file to validate bindings to the !name in the script
        // you'll get a warning unless the binding name happens to be the same
        // as the leaf file name, which it sometimes is
        scriptName = file.getFileNameWithoutExtension();
        log.add("Warning: Script file not found: " + juce::String(ref->getFile()));
        // adds log clutter and doesn't help 
        //log.add("  Assuming bindings reference the name: " + scriptName);
    }
    else {
        log.add("Verified script file: " + juce::String(ref->getFile()));
        scriptName = getScriptName(file);
    }

    if (scriptName.length() > 0)
      scriptNames.add(scriptName);
    
    return scriptName;
}

/**
 * When the ScriptConfig contains a directory reference, we just keep the
 * single ScriptRef, but for binding verification we need to descend into it
 * and load all the script names it contains.
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
 *
 * OG Mobius supported the concept of "overlay" bindings, which are not
 * fully implemented yet, but we'll bring them in.  The first BindingConfig
 * is normally named "Common Bindings" and is always active.  Subsequent
 * BindingConfigs are overlays and could be individually selected and
 * merged with the common bindings.
 *
 * On upgrade, each BindingConfig is name matched with a BindingSet in the
 * new model.  If a name is not found, a new one is created.  Within each
 * new BindingSet, Bindings that already exist in the new model are filtered.
 * Installation is then a merge of the new BindingSets and the existing ones
 * rather than a full replacement like Presets and Setups.  The special case
 * is the first BindingConfig which is always merged with the first BindingSet
 * in the new model regardless of name.  
 */
void UpgradePanel::loadBindings()
{
    // the first one is special, it doesn't requirea name match
    BindingSet* oldBindings = mobiusConfig->getBindingSets();
    if (oldBindings != nullptr) {
        
        BindingSet* masterBindings = masterConfig->getBindingSets();
        newBindingSets.add(loadBindings(oldBindings, masterBindings));

        // the rest require name matching
        BindingSet* masterOverlays = nullptr;
        if (masterBindings != nullptr)
          masterOverlays = masterBindings->getNextBindingSet();
        
        oldBindings = oldBindings->getNextBindingSet();
        while (oldBindings != nullptr) {
            // locate an existing set by name
            BindingSet* master = nullptr;
            for (BindingSet* set = masterOverlays ; set != nullptr ; set = set->getNextBindingSet()) {
                if (StringEqual(oldBindings->getName(), set->getName())) {
                    master = set;
                    break;
                }
            }

            newBindingSets.add(loadBindings(oldBindings, master));
            oldBindings = oldBindings->getNextBindingSet();
        }
    }
}

BindingSet* UpgradePanel::loadBindings(BindingSet* old, BindingSet* master)
{
    BindingSet* newBindings = new BindingSet();
    // if this is the first one, the name doesn't matter
    // it will always be installed in the first master BindingSet
    newBindings->setName(old->getName());
    
    int midiCount = 0;
    int keyCount = 0;
    int hostCount = 0;
    int buttonCount = 0;
            
    int midiAdded = 0;
    int hostAdded = 0;
    int keyAdded = 0;
    int buttonAdded = 0;

    log.add("");
    log.add("Upgrading binding set: " + juce::String(old->getName()));
    
    Binding* binding = old->getBindings();
    while (binding != nullptr) {
        Binding* copy = nullptr;
            
        if (binding->trigger == TriggerMidi ||
            binding->isMidi()) {
            midiCount++;

            copy = upgradeBinding(binding);
            if (copy != nullptr) {
                // adjust the channel
                copy->midiChannel = binding->midiChannel + 1;
            }
        }
        else if (binding->trigger == TriggerKey) {
            // these are almost always simple ASCII codes which should
            // transfer from the old keycode space to Juce
            // anything beyond that like function keys and modifiers
            // won't and I don't want to mess with mapping them right now
            // just leave them there so they can be corrected later
            keyCount++;
            copy = upgradeBinding(binding);
        }
        else if (binding->trigger == TriggerHost) {
            hostCount++;
            copy = upgradeBinding(binding);
        }
        else if (binding->trigger == TriggerUI) {
            // this must be a very old MobiusConfig since
            // button bindings were moved to uiconfig.xml
            // this will be converted to a DisplayButton
            buttonCount++;
            copy = upgradeBinding(binding);
            if (copy != nullptr) {
                // these aren't Bindings in the new model
                DisplayButton* db = new DisplayButton();
                db->action = copy->getSymbolName();
                db->arguments = juce::String(copy->getArguments());
                db->scope = juce::String(copy->getScope());
                delete copy;
                copy = nullptr;
                if (!addUpgradeButton(db)) {
                    // don't add log clutter
                    // log.add(juce::String("Binding for button ") + binding->getSymbolName() + " already exists");
                    delete db;
                }
                else {
                    buttonAdded++;
                }
            }
        }
        
        if (copy != nullptr) {
            // it was valid
            Binding* existing = nullptr;
            if (master != nullptr)
              existing = master->findBinding(copy);

            // if we fixed the upgrader and do it again, there might
            // be stale bindings left behind we don't want, it's hard to
            // know what those were unfortunately
            if (existing == nullptr) {
                newBindings->addBinding(copy);
                if (binding->trigger == TriggerHost)
                  hostAdded++;
                else if (binding->trigger == TriggerKey)
                  keyAdded++;
                else
                  midiAdded++;
            }   
            else {
                delete copy;
                // don't add log clutter, may have to upgrade several times
                //log.add(juce::String("Binding for ") + binding->getSymbolName() + " already exists");
            }
        }
        
        binding = binding->getNext();
    }

    if (midiCount > 0) 
      log.add(juce::String(midiCount) + " MIDI bindings loaded, " +
              juce::String(midiAdded) + " new");
    
    if (hostCount > 0)
      log.add(juce::String(hostCount) + " Host Parameter bindings loaded, " +
              juce::String(hostAdded) + " new");
    
    if (keyCount > 0)
      log.add(juce::String(keyCount) + " Keyboard bindings loaded, " +
              juce::String(keyAdded) + " new");
    
    if (buttonCount > 0)
      log.add(juce::String(buttonCount) + " UI Button bindings loaded, " +
              juce::String(buttonAdded) + " new");

    return newBindings;
}

/**
 * Upgrade an old binding, and emit validation messages.
 * Normally this returns a copy of the old binding with
 * the necessary adjustments.  If strict mode is on it
 * will return nullptr if the binding does not resolve
 * to a valid symbol or script name.
 */
Binding* UpgradePanel::upgradeBinding(Binding* src)
{
    Binding* copy = new Binding(src);

    juce::String name (src->getSymbolName());
    Symbol* symbol = Symbols.find(name);
    
    if (symbol != nullptr &&
        (symbol->function != nullptr || symbol->parameter != nullptr)) {
        // a standard name
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
        // a reference to a script where were able to validate the name
        // no further adjustments
    }
    else if (symbol != nullptr && symbol->coreFunction != nullptr) {
        // this is an old core function that didn't map to a new one
        // the binding may work but we should have caught it and
        // renamed it above
        log.add("Warning: Binding to unsupported core function: " + name);
    }
    else if (symbol != nullptr && symbol->coreParameter != nullptr) {
        // like resolved core functions, this might work, but probably
        // not as intended
        log.add("Warning: Binding to unsupported core parameter: " + name);
    }
    else {
        // unresolved reference
        // this may happen if the scripts can't be loaded
        // or if there is an old function alias that isn't supported
        // by the above logic
        log.add("Warning: Unresolved binding name \"" + name + "\"");
        
        // if strict is on, don't add it
        // I used this in testing, but in practice users won't want this
        // since it's usually an unloaded script reference they can correct
        // or an error in my alias handling that I need to fix
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

        if (newScripts.size() > 0) {
            ScriptConfig* masterScripts = masterConfig->getScriptConfig();
            if (masterScripts == nullptr) {
                masterScripts = new ScriptConfig();
                masterConfig->setScriptConfig(masterScripts);
            }

            while (newScripts.size() > 0) {
                ScriptRef* ref = newScripts.removeAndReturn(0);
                masterScripts->add(ref);
            }
        }

        // these are more complicated
        // they are merged into existing sets with the same name
        if (newBindingSets.size() > 0) {
            BindingSet* neu = newBindingSets.removeAndReturn(0);
            BindingSet* masterBindings = masterConfig->getBindingSets();
            if (masterBindings == nullptr) {
                // unusual, bootstrap the default set
                masterBindings = new BindingSet();
                masterConfig->addBindingSet(masterBindings);
            }

            // the first two are always merged regardless of name
            mergeBindings(neu, masterBindings);
            delete neu;

            // the reset are "overlays", they will have been filtered
            BindingSet* masterOverlays = masterBindings->getNextBindingSet();
            while (newBindingSets.size() > 0) {
                neu = newBindingSets.removeAndReturn(0);
                BindingSet* masterDest = nullptr;
 for (BindingSet* set = masterOverlays ; set != nullptr ; set = set->getNextBindingSet()) {
                    if (StringEqual(neu->getName(), set->getName())) {
                        masterDest = set;
                        break;
                    }
                }
                
                if (masterDest != nullptr) {
                    mergeBindings(neu, masterDest);
                    delete neu;
                }
                else {
                    // first time here with a new one
                    // the expectation is that the order is preserved so append it
                    BindingSet* masterLast = masterBindings;
                    while (masterLast->getNext() != nullptr)
                      masterLast = masterLast->getNextBindingSet();
                    masterLast->setNext(neu);
                }
            }
        }
        
        // buttons go in UIConfig
        // since we're not merging into a potentially existing
        // object, we just replace it every time
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

/**
 * Move the filtered Binding list from the upgraded set into
 * a master set.  This one is annoying due to old memory management.
 * We want to move ownership of the objects over to the destination set.
 * Easiest is to do linked list surgery on the destination.
 */
void UpgradePanel::mergeBindings(BindingSet* src, BindingSet* dest)
{
    Binding* list = src->stealBindings();

    Binding* destLast = dest->getBindings();
    if (destLast == nullptr) {
        dest->setBindings(list);
    }
    else {
        while (destLast->getNext() != nullptr)
          destLast = destLast->getNext();
        destLast->setNext(list);
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
