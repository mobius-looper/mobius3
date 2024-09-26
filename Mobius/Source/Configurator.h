/**
 * Utility class to manage configuration files and various things needed throughout
 * the application.  This could just be an interface which would make the dependencies
 * on Supervisor a little more hidden for code below MobiusInterface.
 *
 * The main thing this does is remove dependencies on Supervisor from a large body of code
 * that just needs access to configuration files.  It also provides the SymbolTable which
 * is needed in a few places in the core.  This can also serve as the accessor for the
 * eventual HashMap or juce::ValueTree as we migrate away from Preset and Setup.
 *
 * !! I don't think this was ever used.  Provider now provides the isolation
 *
 */

#pragma once

#include <JuceHeader.h>

class Configurator
{
  public:

    Configurator(class Supervisor* s);
    ~Configurator();

    class DeviceConfig* getDeviceConfig();
    void updateDeviceConfig();
    
    class MobiusConfig* getMobiusConfig();
    void updateMobiusConfig();
    void reloadMobiusConfig();
    
    class UIConfig* getUIConfig();
    void updateUIConfig();
    void reloadUIConfig();
    
    class HelpCatalog* getHelpCatalog();
    
    class DynamicConfig* getDynamicConfig();

    // special accessors for things deep within the engine
    int getActiveSetup();
    int getActivePreset();

    // todo: Some of the things in MobiusContainer like sampleRate and blockSize could
    // go here too...

};

