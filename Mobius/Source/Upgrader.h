/**
 * Utility class encapsulating runtime upgrades to configuration objects read from
 * the XML files to adapt to model changes that no longer match what was stored.
 * Called by Supervisor at startup.
 *
 * Note that this is NOT the interactive upgrade utility that imports Mobius 2.5
 * configuration files.  That is in test/UpgradePanel.
 *
 * Code here is temporary and can be pruned as the user base moves to higher builds.
 *
 */

#pragma once

#include <JuceHeader.h>

class Upgrader
{
  public:

    Upgrader(class Supervisor*s) {
        supervisor = s;
    }

    ~Upgrader() {}

    bool upgrade(class MobiusConfig* config, class MainConfig* neu);

  private:

    class Supervisor* supervisor = nullptr;

    int upgradePort(int number);
    bool upgradeFunctionProperties(class MobiusConfig* config);
    void upgradeFunctionProperty(class StringList* names, bool focus, bool confirm, bool muteCancel);
    bool upgradeGroups(class MobiusConfig* config);
    
    bool refreshMainConfig(class MobiusConfig* old, class MainConfig* neu);
    void convertEnum(const char* name, int value, class ValueSet* dest);
    void convertPreset(class Preset* preset, class MainConfig* main);
    void convertSetup(class Setup* setup, class MainConfig* main);
    void convertSetupTrack(class SetupTrack* track, int trackNumber, class ValueSet* neu);
    
};