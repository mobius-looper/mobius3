
#include <JuceHeader.h>

#include "util/Trace.h"

#include "model/XmlRenderer.h"
#include "model/MobiusConfig.h"

#include "model/UIConfig.h"
#include "model/DeviceConfig.h"
#include "model/SystemConfig.h"
#include "model/StaticConfig.h"
#include "model/HelpCatalog.h"
#include "model/Session.h"

#include "Provider.h"

#include "FileManager.h"

const char* SystemConfigFile = "system.xml";
const char* StaticConfigFile = "static.xml";
const char* DeviceConfigFile = "devices.xml";
const char* MobiusConfigFile = "mobius.xml";
const char* UIConfigFile = "uiconfig.xml";
const char* DefaultSessionFile = "session.xml";
const char* HelpFile = "help.xml";

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

/**
 * Read and parse an XML configuration file, logging the usual errors.
 * Note that the XmlElement returned here is dynamically allocated and
 * must be deleted.  And no, I'm not using std::unique_ptr because I live
 * on the edge motherfucker.
 */
juce::XmlElement* FileManager::readConfigFileRoot(const char* filename, const char* expected)
{
    juce::XmlElement* result = nullptr;

    juce::String xml = readConfigFile(filename);
    if (xml.length() == 0) {
        Trace(2, "FileManager: Missing file %s", filename);
    }
    else {
        juce::XmlDocument doc(xml);
        std::unique_ptr<juce::XmlElement> docel = doc.getDocumentElement();
        if (docel == nullptr) {
            Trace(1, "FileManager: Error parsing %s", filename);
            Trace(1, "  %s", doc.getLastParseError().toUTF8());
        }
        else if (!docel->hasTagName(expected)) {
            Trace(1, "FileManager: Incorrect XML element in file %s", filename);
        }
        else {
            // set it free
            result = docel.release();
        }
    }
    return result;
}

void FileManager::logErrors(const char* filename, juce::StringArray& errors)
{
    if (errors.size() > 0) {
        Trace(1, "FileManager: Errors parsing %s", filename);
        for (auto error : errors)
          Trace(1, "  %s", error.toUTF8());
    }
}

//////////////////////////////////////////////////////////////////////
//
// SystemConfig
//
//////////////////////////////////////////////////////////////////////

SystemConfig* FileManager::readSystemConfig()
{
    SystemConfig* scon = new SystemConfig();
    juce::XmlElement* root = readConfigFileRoot(SystemConfigFile, SystemConfig::XmlElementName);
    if (root != nullptr) {
        juce::StringArray errors;
        scon->parseXml(root, errors);
        logErrors(StaticConfigFile, errors);
        delete root;
    }
    return scon;
}

/**
 * Write a DeviceConfig back to the file system.
 * Ownership of the config object does not transfer.
 */
void FileManager::writeSystemConfig(SystemConfig* config)
{
    if (config != nullptr) {
        juce::String xml = config->toXml();
        writeConfigFile(SystemConfigFile, xml.toUTF8());
    }
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
// Help & StaticConfig
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

StaticConfig* FileManager::readStaticConfig()
{
    StaticConfig* scon = new StaticConfig();
    juce::XmlElement* root = readConfigFileRoot(StaticConfigFile, StaticConfig::XmlElementName);
    if (root != nullptr) {
        juce::StringArray errors;
        scon->parseXml(root, errors);
        logErrors(StaticConfigFile, errors);
        delete root;
    }
    return scon;
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

