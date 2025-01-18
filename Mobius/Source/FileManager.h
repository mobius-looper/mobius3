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

    class DeviceConfig* readDeviceConfig();
    void writeDeviceConfig(class DeviceConfig* config);
    
    class MobiusConfig* readMobiusConfig();
    void writeMobiusConfig(MobiusConfig* c);

    class UIConfig* readUIConfig();
    void writeUIConfig(class UIConfig* config);
    
    class HelpCatalog* readHelpCatalog();
    class SystemConfig* readSystemConfig();
    
    class Session* readSession(const char* filename);
    class Session* readDefaultSession();
    void writeDefaultSession(class Session* ses);
        
  private:

    class Provider* provider = nullptr;

    juce::String readConfigFile(const char* name);
    void writeConfigFile(const char* name, const char* xml);

};
