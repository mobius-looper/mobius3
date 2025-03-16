/**
 * Utility class used by Supervisor once during startup to upgrade mobius.xml
 * and other config files for model changes.
 *
 * This is old and will no longer be used once the Session transition is over.
 * Put nothing new in here. 
 */

#include <JuceHeader.h>

#include "util/List.h"

#include "model/old/MobiusConfig.h"
#include "model/old/Preset.h"
#include "model/old/Setup.h"
#include "model/old/XmlRenderer.h"
#include "model/Session.h"
#include "model/ValueSet.h"
#include "model/Symbol.h"
#include "model/SymbolId.h"
#include "model/FunctionProperties.h"
#include "model/ParameterProperties.h"
#include "model/ParameterSets.h"

#include "Symbolizer.h"
#include "ModelTransformer.h"
#include "Supervisor.h"

/**
 * Kludge to adjust port numbers which were being incorrectly saved 1 based rather
 * than zero based.  Unfortunately this means imported Setups will have to be imported again.
 *
 * Also does the function properties conversion, and normalizes group names.
 */
bool Upgrader::upgrade(MobiusConfig* config)
{
    bool updated = false;
    
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
        updated = true;
    }

    if (upgradeFunctionProperties(config))
      updated = true;
    
    if (upgradeGroups(config))
      updated = true;

    // testing only
    if (upgradePresets(config))
      updated = true;

    return updated;
}

int Upgrader::upgradePort(int number)
{
    // if it's already zero then it has either been upgraded or it hasn't passed
    // through the UI yet
    if (number > 0)
      number--;
    return number;
}

/**
 * Convert the old function name lists into Symbol properties.
 */
bool Upgrader::upgradeFunctionProperties(MobiusConfig* config)
{
    bool updated = false;

    StringList* list = config->getFocusLockFunctions();
    if (list != nullptr && list->size() > 0) {
        upgradeFunctionProperty(list, true, false, false);
        // don't do this again
        config->setFocusLockFunctions(nullptr);
        updated = true;
    }

    list = config->getConfirmationFunctions();
    if (list != nullptr && list->size() > 0) {
        upgradeFunctionProperty(list, false, true, false);
        config->setConfirmationFunctions(nullptr);
        updated = true;
    }
    
    list = config->getMuteCancelFunctions();
    if (list != nullptr && list->size() > 0) {
        upgradeFunctionProperty(list, false, false, true);
        config->setMuteCancelFunctions(nullptr);
        updated = true;
    }

    return updated;
}

void Upgrader::upgradeFunctionProperty(StringList* names, bool focus, bool confirm, bool muteCancel)
{
    if (names != nullptr) {
        for (int i = 0 ; i < names->size() ; i++) {
            const char* name = names->getString(i);
            Symbol* s = supervisor->getSymbols()->find(name);
            if (s == nullptr) {
                Trace(1, "Upgrader::upgradeFunctionProperties Undefined function %s\n", name);
            }
            else if (s->functionProperties == nullptr) {
                // symbols should have been loaded by now, don't bootstrap
                Trace(1, "Upgrader::upgradeFunctionProperties Missing function properties for %s\n", name);
            }
            else {
                if (focus) s->functionProperties->focus = true;
                if (confirm) s->functionProperties->confirmation = true;
                if (muteCancel) s->functionProperties->muteCancel = true;
            }
        }
    }
}

/**
 * Normalize GroupDefinitions and group name references.
 */
bool Upgrader::upgradeGroups(MobiusConfig* config)
{
    bool updated = false;
    
    // add names for prototype definitions that didn't have them
    int ordinal = 0;
    for (auto group : config->dangerousGroups) {
        if (group->name.length() == 0) {
            group->name = GroupDefinition::getInternalName(ordinal);
            updated = true;
        }
        ordinal++;
    }
    
    // the original group definitions by number
    // make sure we have a GroupDefinition object for all the numbers
    int oldGroupCount = config->getTrackGroupsDeprecated();
    // make sure we have at least 2 for some old expectations
    if (oldGroupCount == 0)
      oldGroupCount = 2;
    
    if (oldGroupCount > config->dangerousGroups.size()) {
        ordinal = config->dangerousGroups.size();
        while (ordinal < oldGroupCount) {
            GroupDefinition* neu = new GroupDefinition();
            neu->name = GroupDefinition::getInternalName(ordinal);
            config->dangerousGroups.add(neu);
            updated = true;
            ordinal++;
        }
    }

    // setups used to reference groups by ordinal
    Setup* setup = config->getSetups();
    while (setup != nullptr) {
        SetupTrack* track = setup->getTracks();
        while (track != nullptr) {
            int groupNumber = track->getGroupNumberDeprecated();
            if (groupNumber > 0) {
                if (track->getGroupName().length() > 0) {
                    // already upgraded, stop using the number
                    // hmm, bindings would rather use ordinals, normalize the there too?
                    track->setGroupNumberDeprecated(0);
                }
                else {
                    // this was an ordinal starting from 1
                    int groupIndex = groupNumber - 1;
                    if (groupIndex >= 0 && groupIndex < config->dangerousGroups.size()) {
                        GroupDefinition* def = config->dangerousGroups[groupIndex];
                        track->setGroupName(def->name);
                    }
                    else {
                        // here we could treat these like the old maxGroups count
                        // and synthesize new ones to match
                        Trace(1, "Upgrader::upgradeGroups Setup group reference out of range %d",
                              groupNumber);
                    }
                    // stop using the number
                    track->setGroupNumberDeprecated(0);
                }
                updated = true;
            }
            track = track->getNext();
        }
        setup = setup->getNextSetup();
    }

    return updated;
}

/**
 * Convert the Preset list from the MobiusConfig into the ParameterSets
 * in parameters.xml.
 *
 * This only happens once.  As soon as parameters.xml has a non-empty
 * ParameterSets the upgrade stops.
 */
bool Upgrader::upgradePresets(MobiusConfig* config)
{
    bool updated = false;
    // might want an option for this
    bool forceUpgrade = false;
    
    ParameterSets* sets = supervisor->getParameterSets();

    // testing sets size is unreliable if they happen to delete
    // all of them after upgrading
    //if (sets->getSets().size() == 0 || forceUpgrade) {
    if (!sets->isUpgraded() || forceUpgrade) {
    
        ModelTransformer transformer(supervisor);

        for (Preset* p = config->getPresets() ; p != nullptr ; p = p->getNextPreset()) {

            ValueSet* set = sets->find(juce::String(p->getName()));
            bool isNew = false;
            
            if (set == nullptr) {
                // the usual case unless forceUpgrade
                set = new ValueSet();
                set->name = juce::String(p->getName());
                sets->add(set);
                isNew = true;
            }


            if (forceUpgrade || isNew) {
                // if this is forceUpgrade, it will only overwrite things or
                // add new things, it won't remove things
                transformer.transform(p, set);
                updated = true;
            }
        }

        if (!sets->isUpgraded()) {
            sets->setUpgraded(true);
            updated = true;
        }

        if (updated) {
            // note: do NOT call updateParameterSets which will
            // do propagation and we're not necessarily initialized yet
            FileManager* fm = supervisor->getFileManager();
            fm->writeParameterSets(sets);
        }

        // go the other direction for testing
#if 0    
        MobiusConfig* stubconfig = new MobiusConfig();
        for (auto set : sets->sets) {
            Preset* p = new Preset();
            p->setName(set->name.toUTF8());
            stubconfig->addPreset(p);
            transformer.transform(set, p);
        }
        XmlRenderer xr;
        char* xml = xr.render(stubconfig);
        juce::File root = supervisor->getRoot();
        juce::File file = root.getChildFile("converted.xml");
        file.replaceWithText(juce::String(xml));
        delete xml;
        delete stubconfig;
#endif
    }
    
    return updated;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
