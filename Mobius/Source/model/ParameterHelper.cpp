
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "Symbol.h"
#include "ParameterProperties.h"
#include "MobiusConfig.h"
#include "Structure.h"
#include "Preset.h"
#include "UIConfig.h"

#include "../Provider.h"

#include "ParameterHelper.h"

void ParameterHelper::getStructureNames(Provider* p, Symbol* s, juce::StringArray& result)
{
    if (s->level == LevelUI) {
        UIConfig* config = p->getUIConfig();
        switch (s->id) {
            case ParamActiveLayout: {
                for (auto layout : config->layouts)
                  result.add(layout->name);
            }
                break;

            case ParamActiveButtons: {
                for (auto buttons : config->buttonSets)
                  result.add(buttons->name);
            }
                break;
                
            default:
                Trace(1, "ParameterHelper: Unsupported UI parameter %s", s->getName());
        }
    }
    else {
        MobiusConfig* config = p->getOldMobiusConfig();
        if (s->id == ParamTrackGroup) {
            for (auto group : config->dangerousGroups) {
                result.add(group->name);
            }
        }
        // ActivePreset is used by ParametersElement
        else if (//s->id == ParamTrackPreset ||
                 s->id == ParamActivePreset) {
            for (Preset* preset = config->getPresets() ; preset != nullptr ;
                 preset = preset->getNextPreset()) {
                result.add(juce::String(preset->getName()));
            }
        }
        else {
            Trace(1, "ParameterHelper: Unsupported parameter %s", s->getName());
        }
    }
}

/**
 * Convert a structure ordinal into a name.
 *
 * Used by ParametersElement and for some reason mslQuery in both
 * Supervisor and MobiusKernel.
 */
juce::String ParameterHelper::getStructureName(Provider* p, Symbol* s, int ordinal)
{
    juce::String name;
    
    if (s->level == LevelUI) {
        UIConfig* config = p->getUIConfig();
        switch (s->id) {
            case ParamActiveLayout: {
                if (ordinal >= 0 && ordinal < config->layouts.size())
                  name = config->layouts[ordinal]->name;
            }
                break;
            case ParamActiveButtons: {
                if (ordinal >= 0 && ordinal < config->buttonSets.size())
                  name = config->buttonSets[ordinal]->name;
            }
                break;

            default: {
                Trace(1, "ParameterHelper: Unsupported UI parameter %s\n", s->name.toUTF8());
            }
        }
    }
    else {
        MobiusConfig* config = p->getOldMobiusConfig();
        if (s->id == ParamTrackGroup) {
            if (ordinal >= 0 && ordinal < config->dangerousGroups.size()) {
                GroupDefinition* def = config->dangerousGroups[ordinal];
                name = def->name;
            }
        }
        else if (// s->id == ParamTrackPreset ||
            s->id == ParamActivePreset) {
            Structure* list = config->getPresets();
            Structure* st = Structure::get(list, ordinal);
            if (st != nullptr)
              name = juce::String(st->getName());
        }
        else {
            Trace(1, "ParameterHelper: Unsupported parameter %s", s->getName());
        }
    }
    return name;
}

/**
 * Return the high range of the parameter ordinal.
 * The minimum can be assumed zero.
 *
 * Used by ParametersElement
 */
int ParameterHelper::getParameterMax(Provider* p, Symbol* s)
{
    int max = 0;
    bool handled = false;
    
    if (s->level == LevelUI) {
        UIConfig* config = p->getUIConfig();
        switch (s->id) {
            case ParamActiveLayout: {
                max = config->layouts.size() - 1;
                handled = true;
            }
                break;

            case ParamActiveButtons: {
                max = config->buttonSets.size() - 1;
                handled = true;
            }
                break;
                
            default: break;
        }
    }
    else {
        MobiusConfig* config = p->getOldMobiusConfig();
        if (s->id == ParamTrackGroup) {
            max = config->dangerousGroups.size();
            handled = true;
        }
        else if (// s->id == ParamTrackPreset ||
                 s->id == ParamActivePreset) {
            for (Preset* preset = config->getPresets() ; preset != nullptr ;
                 preset = preset->getNextPreset()) {
                max++;
            }
            handled = true;
        }
    }

    if (!handled) {
        ParameterProperties* props = s->parameterProperties.get();
        if (props != nullptr) {
            if (props->type == TypeEnum) {
                max = props->values.size();
                handled = true;
            }
            else {
                max = props->high;
                handled = true;
            }
        }
    }

    if (!handled) {
        Trace(1, "ParameterHelper::getParameterMax Unsupported parameter %s",
              s->getName());
    }
    
    return max;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
