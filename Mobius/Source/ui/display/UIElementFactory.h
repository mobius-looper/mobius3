/**
 * Handles the creation of a UIElement instance that corresponds to a definition.
 */

#pragma once

class UIElementFactory
{
  public:

    static class UIElement* create(class Provider* p, class UIElementDefinition* def);
};

        
