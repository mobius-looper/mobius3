/**
 * Base implementation of UIParameter.
 * 
 * Generated subclasses and code are found in UIParameterClasses.cpp
 * 
 */

#include <JuceHeader.h>

// some string utils
#include "../util/Util.h"
// for StringList
#include "../util/List.h"

// temporary for dynamic parameters
#include "../model/MobiusConfig.h"
#include "../model/Structure.h"
#include "../model/Preset.h"
#include "../model/Setup.h"
#include "../model/Binding.h"

#include "UIParameter.h"

UIParameter::UIParameter()
{
    // add to the global registry
    ordinal = (int)Instances.size();
    Instances.push_back(this);
}

UIParameter::~UIParameter()
{
}

/**
 * Convert a symbolic parameter value into an ordinal.
 * This could support both internal names and display names
 * but it's only using internal names at the moment.
 *
 * This cannot be used for type=string.
 * For type=structure you must use getDynamicEnumOrdinal
 */
int UIParameter::getEnumOrdinal(const char* value)
{
	int enumOrdinal = -1;

	if (value != nullptr) {
		for (int i = 0 ; values[i] != nullptr ; i++) {
			if (StringEqualNoCase(values[i], value)) {
				enumOrdinal = i;
				break;
			}
		}
    }
    return enumOrdinal;
}

/**
 * Convert an ordinal into the symbolic enumeration name.
 * This almost always comes from an (int) cast of
 * a zero based enum value, but check the range.
 * Will be simpler with vector.
 */ 
const char* UIParameter::getEnumName(int enumOrdinal)
{
    const char* enumName = nullptr;

    if (values != nullptr) {
        int max = 0;
        while (values[max] != nullptr) {
            max++;
        }
        if (enumOrdinal >= 0 && enumOrdinal < max)
          enumName = values[enumOrdinal];
    }
    return enumName;
}

/**
 * Convert an ordinal into the symbolic enumeration label.
 * Labels are usually what is displayed in the UI.
 * getEnumName is what would be in an XML file.
 */ 
const char* UIParameter::getEnumLabel(int enumOrdinal)
{
    const char* label = nullptr;

    if (valueLabels != nullptr) {
        int max = 0;
        while (valueLabels[max] != nullptr) {
            max++;
        }
        if (enumOrdinal >= 0 && enumOrdinal < max)
          label = valueLabels[enumOrdinal];
    }

    if (label == nullptr) {
        // missing or abbreviated label array, fall back to the name
        label = getEnumName(enumOrdinal);
    }
    return label;
}

/**
 * Calculate the maximum ordinal for a structure parameter.
 * Temporary until we get Query fleshed out.
 *
 * We need to deal with all type=structure parameters plus
 * a few integer parameters.
 *
 *   MobiusConfig::activeSetup
 *   MobiusConfig::activeOverlay
 *   SetupTrack::preset
 *
 *   MobiusConfig::activeTrack
 *   Preset::loopCount
 *   SetupTrack::group
 *   SetupTrack::preset
 *
 * !! is the high value inclusive or is it 1 beyond,
 * e.g. is it a list length or the highest zero based index
 * of length-1 ?
 *
 * for MIDI it is 127 which is inclusive so in all the list
 * calculations below make sure to subtract 1
 *
 * aww shit, there are a lot of low=1 parameters and I want
 * ordinals to be consistently zero based, sort this out...
 *
 * Since this is only called by new code, assume zero based ordinals
 * and convert internally in Actionator if necessary
 *
 * where this is going to suck is binding args since users think of
 * 1 as the first track/loop
 */
int UIParameter::getDynamicHigh(MobiusConfig* container)
{
    int dynamicHigh = 0;
    
    // we'll be starting to have this pattern in several places
    // move to lambdas!
    
    if (this == UIParameterActiveTrack) {
        // inconsistency, track ordinals should be zero based
        // are for loops, but this is
        dynamicHigh = container->getTracks() - 1;
    }
    else if (this == UIParameterLoopCount) {
        dynamicHigh = container->getMaxLoops() - 1;
    }
    else if (this == UIParameterGroup) {
        dynamicHigh = container->getTrackGroups() - 1;
    }
    else if (type == TypeStructure) {
        // kludge because GroupDefinitions are not Structures
        if (this == UIParameterGroupName) {
            dynamicHigh = container->groups.size();
        }
        else {
            Structure* list = getStructureList(container);
            dynamicHigh = Structure::count(list) - 1;
        }
    }
    else if (type == TypeEnum) {
        // generated classes did not set high so have to get it fom
        // the values list, sigh
        if (values != nullptr) {
            int count = 0;
            while (values[count] != nullptr) count++;
            dynamicHigh = count - 1;
        }
    }
    else {
        // must be a static parameter, just return the static high
        dynamicHigh = high;
    }

    return dynamicHigh;
}

/**
 * Get one of the Structure lists from the MobiusConfig
 * used to derive properties of this parameter.
 */
Structure* UIParameter::getStructureList(MobiusConfig* container)
{
    Structure* list = nullptr;
    
    if (this == UIParameterDefaultPreset ||
        this == UIParameterActivePreset ||
        this == UIParameterTrackPreset) {

        list = container->getPresets();
    }
    else if (this == UIParameterActiveSetup) {
        
        list = container->getSetups();
    }
    else if (this == UIParameterActiveOverlay) {
        // this is a weird one, we keep overlays on the same list
        // as the master binding set which cannot be deleted
        // this coincidentlly helps with the "none" ordinal problem
        // because ordinal zero will be the master binding set meaning
        // there is no overlay
        list = container->getBindingSets();
    }

    return list;
}


/**
 * There is a really messy problem with BindingSets and "overlays"
 * about consnstency between ordinals and the names, since the
 * master binding set is on the list with ordinal zero.
 * Don't have the energy for this right now but need to get back to this.
 */
StringList* UIParameter::getStructureNames(MobiusConfig* container)
{
    StringList* names = nullptr;

    // regretting these not being Structures
    if (this == UIParameterGroupName) {
        if (container->groups.size() > 0) {
            names = new StringList();
            for (auto group : container->groups) {
                juce::String gname = group->name;
                names->add((const char*)(gname.toUTF8()));
            }
        }
    }
    else {
        Structure* list = getStructureList(container);
        if (list != nullptr) {
            names = new StringList();
            while (list != nullptr) {
                names->add(list->getName());
                list = list->getNext();
            }
        }
    }
    
    return names;
}

int UIParameter::getStructureOrdinal(MobiusConfig* container, const char* structName)
{
    int structOrdinal = -1;

    if (this == UIParameterGroupName) {
        for (int i = 0 ; i < container->groups.size() ; i++) {
            juce::String gname = container->groups[i]->name;
            if (gname == juce::String(structName)) {
                structOrdinal = i;
                break;
            }
        }
    }
    else {
        Structure* list = getStructureList(container);
        if (list != nullptr) {
            structOrdinal = Structure::getOrdinal(list, structName);
        }
    }
    
    return structOrdinal;
}

const char* UIParameter::getStructureName(MobiusConfig* container, int structOrdinal)
{
    const char* structName = nullptr;

    if (this == UIParameterGroupName) {
        if (structOrdinal >= 0 && structOrdinal < container->groups.size()) {
            GroupDefinition* def = container->groups[structOrdinal];
            // fuck, how constant are these names?
            structName = (const char*)(def->name.toUTF8());
        }
    }
    else {
        Structure* list = getStructureList(container);
        if (list != nullptr) {
            Structure* s = Structure::get(list, structOrdinal);
            if (s != nullptr)
              structName = s->getName();
        }
    }
    return structName;
}

//////////////////////////////////////////////////////////////////////
//
// Global Parameter Registry
//
//////////////////////////////////////////////////////////////////////

std::vector<UIParameter*> UIParameter::Instances;

void UIParameter::trace()
{
    for (int i = 0 ; i < Instances.size() ; i++) {
        UIParameter* p = Instances[i];
        // printf("Parameter %s %s %s\n", p->getName(), getEnumLabel(p->type), getEnumLabel(p->scope));
        printf("Parameter %s\n", p->getName());
    }
}

/**
 * Find a Parameter by name
 * This doesn't happen often so we can do a liner search.
 */
UIParameter* UIParameter::find(const char* name)
{
	UIParameter* found = nullptr;
	
	for (int i = 0 ; i < Instances.size() ; i++) {
		UIParameter* p = Instances[i];
		if (StringEqualNoCase(p->getName(), name)) {
            found = p;
            break;
        }
	}
	return found;
}

/**
 * Find a parameter by it's display name.
 * I believe this is used only by the Setup editor.
 */
UIParameter* UIParameter::findDisplay(const char* name)
{
	UIParameter* found = nullptr;
	
	for (int i = 0 ; i < Instances.size() ; i++) {
		UIParameter* p = Instances[i];
		if (StringEqualNoCase(p->getDisplayName(), name)) {
			found = p;
			break;	
		}
	}
	return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
