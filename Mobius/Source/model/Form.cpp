
#include <JuceHeader.h>

#include "ParameterConstants.h"

#include "Form.h"

//////////////////////////////////////////////////////////////////////
//
// Field
//
//////////////////////////////////////////////////////////////////////

/**
 * The root must be a <Tree> element.
 */
void Field::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    (void)errors;
    name = root->getStringAttribute("name");
    displayName = root->getStringAttribute("displayName");
    type = parseType(root->getStringAttribute("type"));
}

/**
 * Parse an XML type name into a UIParameterType enumeration value.
 * This also exists in Symbolizer.
 */
UIParameterType Field::parseType(juce::String value)
{
    UIParameterType ptype = TypeInt;
    
    if (value == "int")
      ptype = TypeInt;
    else if (value == "bool")
      ptype = TypeBool;
    else if (value == "enum")
      ptype = TypeEnum;
    else if (value == "string")
      ptype = TypeString;
    else if (value == "structure")
      ptype = TypeStructure;

    return ptype;
}

//////////////////////////////////////////////////////////////////////
//
// Form
//
//////////////////////////////////////////////////////////////////////

/**
 * The root must be a <Form> element.
 */
void Form::parseXml(juce::XmlElement* root, juce::StringArray& errors)
{
    (void)errors;
    
    name = root->getStringAttribute("name");
    title = root->getStringAttribute("title");
    
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Field")) {
            Field* field = new Field();
            fields.add(field);
            field->parseXml(el, errors);
        }
        else {
            errors.add(juce::String("Form: Unexpected XML tag name: ") + el->getTagName());
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
