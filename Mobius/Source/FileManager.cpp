
#include <JuceHeader.h>

#include "util/Trace.h"

#include "model/XmlRenderer.h"
#include "model/MobiusConfig.h"
#include "model/UIConfig.h"
#include "model/DeviceConfig.h"
#include "model/HelpCatalog.h"
#include "model/Session.h"

#include "Provider.h"

#include "FileManager.h"

const char* DeviceConfigFile = "devices.xml";
const char* MobiusConfigFile = "mobius.xml";
const char* UIConfigFile = "uiconfig.xml";
const char* HelpFile = "help.xml";
const char* DefaultSessionFile = "session.xml";


FileManager::FileManager(Provider* p)
{
    provider = p;
}

FileManager::~FileManager()
{
}
    
//////////////////////////////////////////////////////////////////////
//
// Generic
//
//////////////////////////////////////////////////////////////////////

/**
 * Read the XML for a configuration file.
 */
juce::String FileManager::readConfigFile(const char* name)
{
    juce::String xml;
    juce::File root = provider->getRoot();
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
void FileManager::writeConfigFile(const char* name, const char* xml)
{
    juce::File root = provider->getRoot();
    juce::File file = root.getChildFile(name);
    // juce::String path = file.getFullPathName();
    // WriteFile(path.toUTF8(), xml);
    file.replaceWithText(juce::String(xml));
}

//////////////////////////////////////////////////////////////////////
//
// DeviceConfig
//
//////////////////////////////////////////////////////////////////////

/**
 * Read the device configuration file.
 */
DeviceConfig* FileManager::readDeviceConfig()
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
 * Write a DeviceConfig back to the file system.
 * Ownership of the config object does not transfer.
 */
void FileManager::writeDeviceConfig(DeviceConfig* config)
{
    if (config != nullptr) {
        juce::String xml = config->toXml();
        writeConfigFile(DeviceConfigFile, xml.toUTF8());
    }
}

//////////////////////////////////////////////////////////////////////
//
// MobiusConfig
//
//////////////////////////////////////////////////////////////////////

/**
 * Read the MobiusConfig from.
 * This one uses the ancient XML parser.
 */
MobiusConfig* FileManager::readMobiusConfig()
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
 * Write a MobiusConfig back to the file system.
 * This should only be called to do surgical modifications
 * to the file for an upgrade, it will NOT propagate changes.
 */
void FileManager::writeMobiusConfig(MobiusConfig* config)
{
    if (config != nullptr) {
        XmlRenderer xr;
        char* xml = xr.render(config);
        writeConfigFile(MobiusConfigFile, xml);
        delete xml;
    }
}

//////////////////////////////////////////////////////////////////////
//
// UIConfig
//
//////////////////////////////////////////////////////////////////////

/**
 * Similar read/writer for the UIConfig
 */
UIConfig* FileManager::readUIConfig()
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
 * Write a UIConfig back to the file system.
 * Ownership of the config object does not transfer.
 */
void FileManager::writeUIConfig(UIConfig* config)
{
    if (config != nullptr) {
        juce::String xml = config->toXml();
        writeConfigFile(UIConfigFile, xml.toUTF8());
        config->dirty = false;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Help
//
//////////////////////////////////////////////////////////////////////

/**
 * Get the system help catalog.
 * Unlike the other XML files, this one is ready only.
 */
HelpCatalog* FileManager::readHelpCatalog()
{
    HelpCatalog* help = nullptr;

    juce::String xml = readConfigFile(HelpFile);
    if (xml.length() == 0) {
        Trace(2, "Supervisor: Bootstrapping %s\n", HelpFile);
        help = new HelpCatalog();
    }
    else {
        help = new HelpCatalog();
        help->parseXml(xml);
    }
    return help;
}

//////////////////////////////////////////////////////////////////////
//
// Session
//
//////////////////////////////////////////////////////////////////////

Session* FileManager::readSession(const char* filename)
{
    Session* ses = nullptr;
    
    juce::String xml = readConfigFile(filename);
    if (xml.length() > 0) {
        ses = new Session();
        ses->parseXml(xml);
    }

    return ses;
}

Session* FileManager::readDefaultSession()
{
    // bootstrapping is more complex for these, let Supervisor handle it
    return readSession(DefaultSessionFile);
}

/**
 * Write a MainConfig back to the file system.
 * Ownership of the config object does not transfer.
 */
void FileManager::writeDefaultSession(Session* ses)
{
    if (ses != nullptr) {
        juce::String xml = ses->toXml();
        writeConfigFile(DefaultSessionFile, xml.toUTF8());
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

