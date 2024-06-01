/**
 * Experimental definitions for user-defined variables.
 * These will become Symbols with BehaviorVariable
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/XmlBuffer.h"
#include "../util/XomParser.h"
#include "../util/XmlModel.h"

#include "VariableDefinition.h"

//////////////////////////////////////////////////////////////////////
// VariableDefinition
//////////////////////////////////////////////////////////////////////

VariableDefinition::VariableDefinition()
{
}

VariableDefinition::~VariableDefinition()
{
}

void VariableDefinition::render(XmlBuffer* b)
{
	b->addOpenStartTag(Element);
    b->addAttribute(AttName, name);
    b->closeStartTag(true);
    b->incIndent();

    // sadly, can't do this, "entry" becomes the value not some
    // sort of key/value container
    // for (auto entry : properties) {

    // this didn't work
    // auto iterator (properties);
    // maybe this?
    // auto iterator = properties.begin();

    // this works
    juce::HashMap<juce::String,juce::var>::Iterator iterator(properties);
    
    while (iterator.next()) {
        b->addOpenStartTag(Property);
        b->addAttribute(AttName, iterator.getKey());
        juce::var value = iterator.getValue();
        // here is where the XML representation could get complex to handle
        // things other than String but that requires another level of schema
        // to parse them.  Could use intValue='4' boolValue='true' etc.
        if (!value.isVoid())
          b->addAttribute(AttValue, value.toString());
        b->closeEmptyElement();
    }

    b->decIndent();
    b->addEndTag(Element);
}

void VariableDefinition::parse(XmlElement* e)
{
    name = juce::String(e->getAttribute(AttName));

	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		if (child->isName(Property)) {
            juce::String pname = juce::String(child->getAttribute(AttName));
            juce::var pvalue = juce::var(child->getAttribute(AttValue));
            set(pname, pvalue);
        }
    }
}

juce::var VariableDefinition::get(juce::String propname)
{
    return properties[propname];
}

void VariableDefinition::set(juce::String propname, juce::var value)
{
    properties.set(propname, value);
}

int VariableDefinition::getInt(juce::String propname)
{
    // what does this return if void?
    return (int)get(propname);
}

bool VariableDefinition::getBool(juce::String propname)
{
    // what does this return if void?
    return (bool)get(propname);
}

juce::String VariableDefinition::getString(juce::String propname)
{
    return get(propname).toString();
}

//////////////////////////////////////////////////////////////////////
// VariableDefinitionSet
//////////////////////////////////////////////////////////////////////

VariableDefinitionSet::VariableDefinitionSet()
{
}

VariableDefinitionSet::~VariableDefinitionSet()
{
}

void VariableDefinitionSet::render(XmlBuffer* b)
{
    b->addStartTag(Element);
    b->incIndent();
    for (auto variable : variables) {
        variable->render(b);
    }
    b->decIndent();
    b->addEndTag(Element);
}

void VariableDefinitionSet::parse(XmlElement* e)
{
	for (XmlElement* child = e->getChildElement() ; child != nullptr ; 
		 child = child->getNextElement()) {

		if (child->isName(VariableDefinition::Element)) {
            VariableDefinition* var = new VariableDefinition();
            var->parse(child);
            variables.add(var);
        }
    }
}

/**
 * Only the top level container needs to implement this
 * interface.  The children only need protected methods
 * that use XmlBuffer and XmlElement.
 */
juce::String VariableDefinitionSet::toXml()
{
    XmlBuffer b;
    render(&b);
    return juce::String(b.getString());
}

void VariableDefinitionSet::parseXml(juce::String xml)
{
	XomParser parser;
    XmlDocument* doc = parser.parse(xml.toUTF8());
    if (doc != nullptr) {
        XmlElement* e = doc->getChildElement();
        if (e->isName(Element)) {
            parse(e);
        }
    }
    delete doc;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
