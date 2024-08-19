/**
 * Utility class used by Supervisor once during startup to upgrade mobius.xml
 * and other config files for model changes.
 */

#include <JuceHeader.h>

#include "util/List.h"
#include "model/MobiusConfig.h"
#include "model/Setup.h"
#include "model/Symbol.h"
#include "model/FunctionProperties.h"

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

    // todo: there is one name list parameter property: resetRetains
    // could handle that here too, but these were less common and it's actually
    // a Setup parameter so there could be more than one, make the user think about
    // and set the new properties manually

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
    for (auto group : config->groups) {
        if (group->name.length() == 0) {
            group->name = GroupDefinition::getInternalName(ordinal);
            updated = true;
        }
        ordinal++;
    }
    
    // the original group definitions by number
    // make sure we have a GroupDefinition object for all the numbers
    int oldGroupCount = config->getTrackGroups();
    // make sure we have at least 2 for some old expectations
    if (oldGroupCount == 0)
      oldGroupCount = 2;
    
    if (oldGroupCount > config->groups.size()) {
        ordinal = config->groups.size();
        while (ordinal < oldGroupCount) {
            GroupDefinition* neu = new GroupDefinition();
            neu->name = GroupDefinition::getInternalName(ordinal);
            config->groups.add(neu);
            updated = true;
            ordinal++;
        }
    }

    // setups used to reference groups by ordinal
    Setup* setup = config->getSetups();
    while (setup != nullptr) {
        SetupTrack* track = setup->getTracks();
        while (track != nullptr) {
            int groupNumber = track->getGroupNumber();
            if (groupNumber > 0) {
                if (track->getGroupName().length() > 0) {
                    // already upgraded, stop using the number
                    // hmm, bindings would rather use ordinals, normalize the there too?
                    track->setGroupNumber(0);
                }
                else {
                    // this was an ordinal starting from 1
                    int groupIndex = groupNumber - 1;
                    if (groupIndex >= 0 && groupIndex < config->groups.size()) {
                        GroupDefinition* def = config->groups[groupIndex];
                        track->setGroupName(def->name);
                    }
                    else {
                        // here we could treat these like the old maxGroups count
                        // and synthesize new ones to match
                        Trace(1, "Upgrader::upgradeGroups Setup group reference out of range %d",
                              groupNumber);
                    }
                    // stop using the number
                    track->setGroupNumber(0);
                }
                updated = true;
            }
            track = track->getNext();
        }
        setup = setup->getNextSetup();
    }

    return updated;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
