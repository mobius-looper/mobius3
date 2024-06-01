/**
 * A catalog of dynamic help text.
 * 
 */

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "HelpCatalog.h"

juce::String HelpCatalog::get(const char* key)
{
    return get(juce::String(key));
}

juce::String HelpCatalog::get(juce::String key)
{
    return catalog[key];
}

/**
 * Use the old XmlBuffer, but need to rewrite this to use juce::String
 * with fewer and more useful options.
 *
 * In practice I won't be using this since help.xml is read-only and formatted
 * with sections and comments that will be lost after parsing.
 */
juce::String HelpCatalog::toXml()
{
    juce::XmlElement root ("HelpCatalog");
    juce::HashMap<juce::String,juce::String>::Iterator i (catalog);
    while (i.next()) {
        juce::XmlElement* el = new juce::XmlElement("Help");
        el->setAttribute("name", i.getKey());
        juce::String text = i.getValue();
        if (!text.containsChar('\n'))
          el->setAttribute("text", text);
        else {
            // see getElementText for the model here
            // note that you can't make an XmlElement without a tag name
            // have to use addTextElement
            el->addTextElement(text);
        }
        root.addChildElement(el);
    }
    return root.toString();
}

/**
 * For when something is fucked with XML
 * Not getting the final line for some reason...
 */
void HelpCatalog::trace()
{
    Trace(2, "HelpCatalog\n");
    juce::HashMap<juce::String,juce::String>::Iterator i (catalog);
    while (i.next()) {
        Trace(2, "  %s\n", i.getKey().toUTF8());
        Trace(2, "    %s\n", i.getValue().toUTF8());
    }
}

/**
 * Let's try using the Juce XML parser rather than my old one.
 */
void HelpCatalog::parseXml(juce::String xml)
{
    juce::XmlDocument doc(xml);
    std::unique_ptr<juce::XmlElement> root = doc.getDocumentElement();
    if (root == nullptr) {
        xmlError("XML parse error: %s\n", doc.getLastParseError());
    }
    else if (!root->hasTagName("HelpCatalog")) {
        xmlError("Unexpected XML tag name: %s\n", root->getTagName());
    }
    else {
        for (auto* el : root->getChildIterator()) {
            if (!el->hasTagName("Help")) {
                xmlError("Unexpected XML tag name: %s\n", el->getTagName());
            }
            else {
                juce::String name = el->getStringAttribute("name");
                if (name.length() == 0) {
                    xmlError("Missing Help element name\n");
                }
                else {
                    juce::String shortText = el->getStringAttribute("text");
                    juce::String longText = getElementText(el);
                    
                    if (longText.length() > 0) {
                        catalog.set(name, longText);
                    }
                    else if (shortText.length() > 0) {
                        catalog.set(name, shortText);
                    }
                    else {
                        xmlError("Help element with no text\n");
                    }
                }
            }
        }
    }
}

/**
 * If element content has text with no embedded elements, it is still
 * represented as a child element.  You can't call getText() on the
 * containing XmlElement.
 *
 * The child elemement(s) will have no name, isTextElement returns true
 * and you can call getText() on it.  In complex documents you can have
 * XML elements embedded within the text, which results in a model
 * with multiple unnamed text elements broken up by named elements.
 * HelpCatalog doesn't allow this.
 * 
 */
juce::String HelpCatalog::getElementText(juce::XmlElement* el)
{
    juce::String text;

    int children = el->getNumChildElements();
    if (children > 0) {
        // we have the expected 1, if there is more then it's
        // an error for help files
        if (children > 1)
          Trace(1, "HelpCatalog: File has element content with embedded elements\n");

        // this handles the child text element, probably merges them
        // if there are embdded elements the tags are thrown out
        text = el->getAllSubText();

        // common to heave leading whitespace if this was indented so trim
        // also common to have interior indentation to make multiple lines
        // look like a paragraph, that's harder to remove.  Have to break the string
        // up by newlines and trim each one
        text = text.trim();
    }
    return text;
}
    
void HelpCatalog::xmlError(const char* msg, juce::String arg)
{
    juce::String fullmsg ("HelpCatalog: " + juce::String(msg));
    if (arg.length() == 0)
      Trace(1, fullmsg.toUTF8());
    else
      Trace(1, fullmsg.toUTF8(), arg.toUTF8());
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

            
