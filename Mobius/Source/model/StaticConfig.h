/**
 * Definitions for system characteristics that are not editable, but are more
 * convenient kept outside the compiled code.
 *
 * This will be stored in the static.xml file.
 *
 * This is a funny one that started as a home for tree and form definitions
 * for the SessionEditor.  Things in here drive parts of the UI but unlike UIConfig,
 * you can't edit them, and are normally only changed on releases.
 *
 * As such it's more like HelpCatalog, and arguably HelpCatalog (and help.xml) could
 * be inside the StaticConfig.  But help is much larger and more convenient to keep
 * outside and in a different file.
 *
 * Other things that could go in here in time are the definitions of the buit-in
 * UIElementDefinitions.
 *
 * Nothing in here currently relates to the engine.
 * 
 */

#pragma once

#include "TreeForm.h"
#include "Form.h"

class StaticConfig
{
  public:

    constexpr static const char* XmlElementName = "StaticConfig";
    
    void parseXml(juce::XmlElement* root, juce::StringArray& errors);

    TreeNode* getTree(juce::String name);
    TreeForm* getTreeForm(juce::String name);
    Form* getForm(juce::String name);
    
  private:

    juce::OwnedArray<TreeNode> trees;
    juce::OwnedArray<TreeForm> treeForms;
    juce::OwnedArray<Form> forms;
    
    juce::HashMap<juce::String,TreeNode*> treeMap;
    juce::HashMap<juce::String,TreeForm*> treeFormMap;

};
