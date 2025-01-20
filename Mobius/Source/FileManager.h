/**
 * A Supervisor subcomponent that deals with reading and writing
 * configuration files.
 */

#pragma once

class FileManager
{
  public:

    FileManager(class Provider* p);
    ~FileManager();

    class SystemConfig* readSystemConfig();
    void writeSystemConfig(class SystemConfig* config);
    
    class DeviceConfig* readDeviceConfig();
    void writeDeviceConfig(class DeviceConfig* config);
    
    class MobiusConfig* readMobiusConfig();
    void writeMobiusConfig(MobiusConfig* c);

    class UIConfig* readUIConfig();
    void writeUIConfig(class UIConfig* config);
    
    class StaticConfig* readStaticConfig();
    class HelpCatalog* readHelpCatalog();
    
  private:

    class Provider* provider = nullptr;

    juce::String readConfigFile(const char* name);
    void writeConfigFile(const char* name, const char* xml);

    juce::XmlElement* readConfigFileRoot(const char* filename, const char* expected);
    void logErrors(const char* filename, juce::StringArray& errors);

};
