
#include <JuceHeader.h>

#include "TreeForm.h"

#include "SystemConfig.h"

void SystemConfig::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Tree")) {
            TreeNode* tree = new TreeNode();
            trees.add(tree);
            tree->parseXml(el, errors);
            if (tree->name.length() == 0)
              errors.add(juce::String("SystemConfig: Tree without name"));
            else
              treeMap.set(tree->name, tree);
        }
        else if (el->hasTagName("Form")) {
            TreeForm* form = new TreeForm();
            forms.add(form);
            form->parseXml(el, errors);
            if (form->name.length() == 0)
              errors.add(juce::String("SystemConfig: Form without name"));
            else
              formMap.set(form->name, form);
        }
        else {
            errors.add(juce::String("SystemConfig: Unexpected XML tag name: " + el->getTagName()));
        }
    }
}

TreeNode* SystemConfig::getTree(juce::String name)
{
    return treeMap[name];
}

TreeForm* SystemConfig::getForm(juce::String name)
{
    return formMap[name];
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
