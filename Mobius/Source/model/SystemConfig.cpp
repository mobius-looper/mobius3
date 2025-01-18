
#include <JuceHeader.h>

#include "../util/Trace.h"

#include "SystemConfig.h"

void SystemConfig::parseXml(juce::String xml)
{
    juce::XmlDocument doc(xml);
    std::unique_ptr<juce::XmlElement> root = doc.getDocumentElement();
    if (root == nullptr) {
        xmlError("Parse parse error: %s\n", doc.getLastParseError());
    }
    else if (!root->hasTagName("SystemConfig")) {
        xmlError("Unexpected XML tag name: %s\n", root->getTagName());
    }
    else {
        // no attributes
        // build metadata might be interesting but there isn't much of that
        // and I'd rather keep the build number compiled in
        
        for (auto* el : root->getChildIterator()) {
            if (el->hasTagName("Category")) {
                Category* cat = parseCategory(el);
                categories.add(cat);
                if (cat->name.length() > 0) 
                  categoryMap.set(cat->name, cat);
            }
            else {
                xmlError("Unexpected XML tag name: %s\n", el->getTagName());
            }
        }
    }
}

void SystemConfig::xmlError(const char* msg, juce::String arg)
{
    juce::String fullmsg ("SystemConfig: " + juce::String(msg));
    if (arg.length() == 0)
      Trace(1, fullmsg.toUTF8());
    else
      Trace(1, fullmsg.toUTF8(), arg.toUTF8());
}

SystemConfig::Category* SystemConfig::parseCategory(juce::XmlElement* root)
{
    Category* category = new Category();
    category->name = root->getStringAttribute("name");
    category->formTitle = root->getStringAttribute("formTitle");
    return category;
}

SystemConfig::Category* SystemConfig::getCategory(juce::String name)
{
    return categoryMap[name];
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
