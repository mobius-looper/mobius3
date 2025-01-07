/**
 * Base class for popup windows that edit UIElements.
 *
 * BasePanel have been managed by PanelFactor and opened from the main menu.
 * ElementEditors I think I'd like to be a bit more independent.
 *
 * They open when you right click on a UIElement and are closly tied to one.
 *
 * You don't need to open them with an action so PanelFactory doesn't need to know about them.
 *
 * 
 */

#pragma once

#include <JuceHeader.h>
#include "BasePanel.h"

class ElementEditor : public BasePanel
{
  public:

    ElementEditor(Provider* p);
    ~ElementEditor();

};
