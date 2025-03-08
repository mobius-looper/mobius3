
#include <JuceHeader.h>

#include "TreeForm.h"
#include "Form.h"

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
            treeForms.add(form);
            form->parseXml(el, errors);
            if (form->name.length() == 0)
              errors.add(juce::String("StaticConfig: TreeForm without name"));
            else
              treeFormMap.set(form->name, form);
        }
        else if (el->hasTagName("VForm")) {
            // temporary, would rather this one be "Form" and the other one "TreeForm"
            Form* form = new Form();
            forms.add(form);
            form->parseXml(el, errors);
            if (form->name.length() == 0)
              errors.add(juce::String("StaticConfig: Form without name"));
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

TreeForm* StaticConfig::getTreeForm(juce::String name)
{
    return treeFormMap[name];
}

Form* StaticConfig::getForm(juce::String name)
{
    Form* found = nullptr;
    for (auto f : forms) {
        if (f->name == name) {
            found = f;
            break;
        }
    }
    return found;
}


/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
