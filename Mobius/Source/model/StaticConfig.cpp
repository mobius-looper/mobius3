
#include <JuceHeader.h>

#include "TreeForm.h"

#include "StaticConfig.h"

void StaticConfig::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Tree")) {
            TreeNode* tree = new TreeNode();
            trees.add(tree);
            tree->parseXml(el, errors);
            if (tree->name.length() == 0)
              errors.add(juce::String("StaticConfig: Tree without name"));
            else
              treeMap.set(tree->name, tree);
        }
        else if (el->hasTagName("Form")) {
            TreeForm* form = new TreeForm();
            forms.add(form);
            form->parseXml(el, errors);
            if (form->name.length() == 0)
              errors.add(juce::String("StaticConfig: Form without name"));
            else
              formMap.set(form->name, form);
        }
        else {
            errors.add(juce::String("StaticConfig: Unexpected XML tag name: " + el->getTagName()));
        }
    }
}

TreeNode* StaticConfig::getTree(juce::String name)
{
    return treeMap[name];
}

TreeForm* StaticConfig::getForm(juce::String name)
{
    return formMap[name];
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
