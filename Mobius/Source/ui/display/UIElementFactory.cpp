/**
 * This is where the mapping between type names from the UIElementDefinition
 * to the concrete UIElement subclass happens.
 *
 * This shoudl eventually be used for all StatusArea and TrackStrip elements but
 * is currently limited to just a few of the new ones like MetronomeElement, and
 * the customizable ones like UIElementLight.
 */

#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/UIConfig.h"

#include "UIElement.h"
#include "TransportElement.h"
#include "MidiSyncElement.h"
#include "UIElementLight.h"
#include "UIElementText.h"
#include "UIElementFactory.h"

/**
 * Create an appropriate UIElement object to implement the visualization
 * defined in the UIElementDefinition.
 */
UIElement* UIElementFactory::create(Provider* p, UIElementDefinition* def)
{
    UIElement* element = nullptr;

    // for some the visualizers are implicit
    if (def->name == "Transport") {
        element = new TransportElement(p, def);
    }
    else if (def->name == "MidiSync") {
        element = new MidiSyncElement(p, def);
    }
    else {
        // for others, the visualization is part of the definition
        if (def->visualizer == "Light") {
            element = new UIElementLight(p, def);
        }
        else if (def->visualizer == "Text") {
            element = new UIElementText(p, def);
        }
        else {
            Trace(1, "UIElement: Unknown element visualizer %s", def->visualizer.toUTF8());
        }
    }
    
    return element;
}

