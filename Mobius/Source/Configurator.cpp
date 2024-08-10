
#include <JuceHeader.h>

#include "Supervisor.h"
#include "Configurator.h"

Configurator::Configurator(class Supervisor* s)
{
}

Configurator::~Configurator()
{
}

//////////////////////////////////////////////////////////////////////
//
// Configuration Files
//
// Still using our old File utiltiies, need to use Juce
//
//////////////////////////////////////////////////////////////////////

const char* DeviceConfigFile = "devices.xml";
const char* MobiusConfigFile = "mobius.xml";
const char* UIConfigFile = "uiconfig.xml";
const char* HelpFile = "help.xml";

/**
 * Read the XML for a configuration file.
 */
juce::String Supervisor::readConfigFile(const char* name)
{
    juce::String xml;
    juce::File root = rootLocator.getRoot();
    juce::File file = root.getChildFile(name);
    if (file.existsAsFile()) {
        Tracej("Reading configuration file " + file.getFullPathName());
        xml = file.loadFileAsString();
    }
    return xml;
}

/**
 * Write an XML configuration file.
 */
void Supervisor::writeConfigFile(const char* name, const char* xml)
{
    juce::File root = rootLocator.getRoot();
    juce::File file = root.getChildFile(name);
    // juce::String path = file.getFullPathName();
    // WriteFile(path.toUTF8(), xml);
    file.replaceWithText(juce::String(xml));
}

/**
 * Read the device configuration file.
 */
DeviceConfig* Supervisor::readDeviceConfig()
{
    DeviceConfig* config = nullptr;
    
    juce::String xml = readConfigFile(DeviceConfigFile);
    if (xml.length() == 0) {
        Trace(2, "Supervisor: Bootstrapping %s\n", DeviceConfigFile);
        config = new DeviceConfig();
    }
    else {
        config = new DeviceConfig();
        config->parseXml(xml);
    }

    return config;
}

/**
 * Read the MobiusConfig from.
 */
MobiusConfig* Supervisor::readMobiusConfig()
{
    MobiusConfig* config = nullptr;
    
    juce::String xml = readConfigFile(MobiusConfigFile);
    if (xml.length() == 0) {
        Trace(2, "Supervisor: Bootstrapping %s\n", MobiusConfigFile);
        config = new MobiusConfig();
    }
    else {
        XmlRenderer xr;
        config = xr.parseMobiusConfig(xml.toUTF8());
        // todo: capture or trace parse errors
    }
    return config;
}

/**
 * Similar read/writer for the UIConfig
 */
UIConfig* Supervisor::readUIConfig()
{
    UIConfig* config = nullptr;
    
    juce::String xml = readConfigFile(UIConfigFile);
    if (xml.length() == 0) {
        Trace(2, "Supervisor: Bootstrapping %s\n", UIConfigFile);
        config = new UIConfig();
    }
    else {
        config = new UIConfig();
        config->parseXml(xml);
    }
    return config;
}

/**
 * Write a DeviceConfig back to the file system.
 * Ownership of the config object does not transfer.
 */
void Supervisor::writeDeviceConfig(DeviceConfig* config)
{
    if (config != nullptr) {
        juce::String xml = config->toXml();
        writeConfigFile(DeviceConfigFile, xml.toUTF8());
    }
}

/**
 * Write a MobiusConfig back to the file system.
 */
void Supervisor::writeMobiusConfig(MobiusConfig* config)
{
    if (config != nullptr) {
        XmlRenderer xr;
        char* xml = xr.render(config);
        writeConfigFile(MobiusConfigFile, xml);
        delete xml;
    }
}

/**
 * Write a UIConfig back to the file system.
 * Ownership of the config object does not transfer.
 */
void Supervisor::writeUIConfig(UIConfig* config)
{
    if (config != nullptr) {
        juce::String xml = config->toXml();
        writeConfigFile(UIConfigFile, xml.toUTF8());
        config->dirty = false;
    }
}

/**
 * Called by components to obtain the MobiusConfig file.
 * The object remains owned by the Supervisor and must not be deleted.
 * For now we will allow it to be modified by the caller, but to save
 * it and propagate change it must call updateMobiusConfig.
 * Caller is responsible for copying it if it wants to make temporary
 * changes.
 *
 * todo: the Proper C++ Way seems to be to pass around the std::unique_pointer
 * rather than calling .get on it.  I suppose it helps drive the point
 * home about ownership, but I find it ugly.
 */
MobiusConfig* Supervisor::getMobiusConfig()
{
    // bool operator tests for nullness of the pointer
    if (!mobiusConfig) {
        MobiusConfig* neu = readMobiusConfig();
        if (neu == nullptr) {
            // bootstrap one so we don't have to keep checking
            neu = new MobiusConfig();
        }
        
        upgrade(neu);
        
        mobiusConfig.reset(neu);
    }
    return mobiusConfig.get();
}

/**
 * Kludge to adjust port numbers which were being incorrectly saved 1 based rather
 * than zero based.  Unfortunately this means imported Setups will have to be imported again.
 */
void Supervisor::upgrade(MobiusConfig* config)
{
    if (config->getVersion() < 1) {
        for (Setup* s = config->getSetups() ; s != nullptr ; s = s->getNextSetup()) {
            // todo: only do this for the ones we know weren't upgraded?
            for (SetupTrack* t = s->getTracks() ; t != nullptr ; t = t->getNext()) {
                t->setAudioInputPort(upgradePort(t->getAudioInputPort()));
                t->setAudioOutputPort(upgradePort(t->getAudioOutputPort()));
                t->setPluginInputPort(upgradePort(t->getPluginInputPort()));
                t->setPluginOutputPort(upgradePort(t->getPluginOutputPort()));
            }
        }
        config->setVersion(1);
    }
}

int Supervisor::upgradePort(int number)
{
    // if it's already zero then it has either been upgraded or it hasn't passed
    // through the UI yet
    if (number > 0)
      number--;
    return number;
}

/**
 * Same dance for the UIConfig
 */
UIConfig* Supervisor::getUIConfig()
{
    if (!uiConfig) {
        UIConfig* neu = readUIConfig();
        if (neu == nullptr) {
            neu = new UIConfig();
        }
        uiConfig.reset(neu);
    }
    return uiConfig.get();
}

DeviceConfig* Supervisor::getDeviceConfig()
{
    if (!deviceConfig) {
        DeviceConfig* neu = readDeviceConfig();
        if (neu == nullptr) {
            // bootstrap one so we don't have to keep checking
            neu = new DeviceConfig();
        }
        deviceConfig.reset(neu);
    }
    return deviceConfig.get();
}

/**
 * Save a modified MobiusConfig, and propagate changes
 * to the interested components.
 * In practice this should only be called by ConfigEditors.
 *
 * The object returned by getMobiusConfig is expected to have
 * been movidied and will be sent to Mobius after writing the file.
 *
 * There are two transient flags inside MobiusConfig that must be
 * set by the PresetEditor and SetupEditor to indiciate that changes
 * were made to those objects.  This is necessary to get the Mobius engine
 * to actually use the new objects.  This prevents needlessly reconfiguring
 * the* engine and losing runtime parameter values if all you change
 * are bindings or global parameters.
 *
 * It's kind of kludgey but gets the job done.  Once the changes have been
 * propagated clear the flags so we don't do it again.
 */
void Supervisor::updateMobiusConfig()
{
    if (mobiusConfig) {
        MobiusConfig* config = mobiusConfig.get();

        writeMobiusConfig(config);

        // reset this so we get a fresh one on next use
        // to reflect potential changes to Script actions
        dynamicConfig.reset(nullptr);

        // propagate config changes to other components
        propagateConfiguration();

        // send it down to the engine
        if (mobius != nullptr)
          mobius->reconfigure(config);

        // clear speical triggers for the engine now that it is done
        config->setupsEdited = false;
        config->presetsEdited = false;
        
        configureBindings(config);
    }
}

/**
 * Added for UpgradePanel
 * Reload the entire MobiusConfig from the file and
 * notify as if it had been edited.
 */
void Supervisor::reloadMobiusConfig()
{
    mobiusConfig.reset(nullptr);
    (void)getMobiusConfig();
    
    propagateConfiguration();
        
    MobiusConfig* config = mobiusConfig.get();
    if (mobius != nullptr)
      mobius->reconfigure(config);
        
    configureBindings(config);
}

void Supervisor::updateUIConfig()
{
    if (uiConfig) {
        UIConfig* config = uiConfig.get();
        
        writeUIConfig(config);

        propagateConfiguration();
    }
}

/**
 * Added for UpgradePanel
 */
void Supervisor::reloadUIConfig()
{
    uiConfig.reset(nullptr);
    (void)getUIConfig();
    
    propagateConfiguration();
}

void Supervisor::updateDeviceConfig()
{
    if (deviceConfig) {
        DeviceConfig* config = deviceConfig.get();
        writeDeviceConfig(config);
    }
}

/**
 * Get the system help catalog.
 * Unlike the other XML files, this one is ready only.
 */
HelpCatalog* Supervisor::getHelpCatalog()
{
    if (!helpCatalog) {
        juce::String xml = readConfigFile(HelpFile);
        HelpCatalog* help = new HelpCatalog();
        help->parseXml(xml);
        helpCatalog.reset(help);
    }
    return helpCatalog.get();
}

/**
 * Old interface we don't use anymore.
 * I don't see this being useful in the future so weed it.
 */
DynamicConfig* Supervisor::getDynamicConfig()
{
    if (!dynamicConfig) {
        dynamicConfig.reset(mobius->getDynamicConfig());
    }
    return dynamicConfig.get();
}

/**
 * Propagate changes through the UI stack after a configuration object
 * has changed.
 */
void Supervisor::propagateConfiguration()
{
    mainWindow->configure();
}

