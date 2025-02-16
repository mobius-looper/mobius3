/**
 * Trees and Forms are read-only we do not need to worry about editing
 * them or user's defining them yet.
 */

#include <JuceHeader.h>

#include "TreeForm.h"

//////////////////////////////////////////////////////////////////////
//
// Tree
//
//////////////////////////////////////////////////////////////////////

/**
 * The root must be a <Tree> element.
 */
void TreeNode::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    name = root->getStringAttribute("name");
    formName = root->getStringAttribute("form");

    juce::String csv = root->getStringAttribute("symbols");
    if (csv.length() > 0)
      symbols = juce::StringArray::fromTokens(csv, ",", "");

    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Tree")) {
            TreeNode* child = new TreeNode();
            nodes.add(child);
            child->parseXml(el, errors);
        }
        else if (el->hasTagName("Form")) {
            if (form != nullptr)
              errors.add(juce::String("TreeNode: Node ") + name + " already has a form");
            else {
                form.reset(new TreeForm());
                form->parseXml(el, errors);
            }
        }
        else {
            errors.add(juce::String("TreeNode: Unexpected XML tag name: ") + el->getTagName());
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Form
//
//////////////////////////////////////////////////////////////////////

/**
 * The root must be a <Form> element.
 */
void TreeForm::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    (void)errors;
    
    name = root->getStringAttribute("name");
    title = root->getStringAttribute("title");
    suppressPrefix = root->getStringAttribute("suppressPrefix");
    
    juce::String csv = root->getStringAttribute("symbols");
    if (csv.length() > 0)
      symbols = juce::StringArray::fromTokens(csv, ",", "");
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
