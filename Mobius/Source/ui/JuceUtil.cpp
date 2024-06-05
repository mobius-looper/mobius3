/**
 * Random debugging shit to figure out Juce.
 */

#include <typeinfo>

#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../util/Vbuf.h"

#include "JuceUtil.h"

void JuceUtil::dumpComponent(const char* title, juce::Component* c, int indent)
{
    trace("*** %s\n", title);
    dumpComponent(c, indent);
}

/**
 * Saw this on the forum for getting the class name of a Component that
 * doesn't have a name.
 * Commentor said this is "really slow to execute" so it should be in
 * debug code only.
 *
 * void print (Component* component) { std::cout << typeid (*component).name() << std::endl; }
 *
 * Fuck yeah, typeid
 * https://en.cppreference.com/w/cpp/language/typeid
 * needs include <typeinfo>
 *
 * it is considered an operator, example:
 * const std::type_info& ti1 = typeid(A);
 *
 * type_info has ==, before, hash_code, and name
 * So for our purposes name is the only thing of interest
 * name is a const char*
 *
 * Not sure what the lifespan of that is so wrap it in a juce::String
 * Not sure why we have to dereference the object pointer
 */
juce::String JuceUtil::getComponentClassName(juce::Component* c)
{
    return juce::String(typeid(*c).name());
}

/**
 * Dump a component hierarchy.
 * Would be nice to figure out typeid or dynamic cast or
 * some fucking way to get the text class name like debuggers do.
 */
void JuceUtil::dumpComponent(juce::Component* c, int indent)
{
    juce::String s;

    for (int i = 0 ; i < indent ; i++)
      s += " ";

    if (c->getName().length() > 0) {
        s += c->getName();
    }
    else {
        // try this
        juce::String className = getComponentClassName(c);
        if (!className.isEmpty())
          s += className;
        else
          s += "???";
    }
    
    s += ": ";
    s += c->getX();
    s += " ";
    s += c->getY();
    s += " ";
    s += c->getWidth();
    s += " ";
    s += c->getHeight();
    if (!c->isVisible()) {
        s += " INVISIBLE";
    }
    s += "\n";

    trace(s.toUTF8());

    indent += 2;

    const juce::Array<juce::Component*>& children = c->getChildren();
    for (int i = 0 ; i < children.size() ; i++) {
        juce::Component* child = children[i];
        dumpComponent(child, indent);
    }
}

/**
 * Convert a String containing a CSV to a StringArray
 */
void JuceUtil::CsvToArray(juce::String csv, juce::StringArray& array)
{
    int start = 0;

    while (start < csv.length()) {
        int comma = csv.indexOfChar(start, ',');
        if (comma < 0) {
            // at the end
            array.add(csv.substring(start));
            start = csv.length();
        }
        else {
            if (comma > start) {
                juce::String token = csv.substring(start, comma);
                // todo: could trim leading and trailing space
                array.add(token);
            }
            // else must have had ",,"
            // not supposed to happen, ignore
            start = comma+1;
        }
    }
}

juce::String JuceUtil::ArrayToCsv(juce::StringArray& array)
{
    juce::String string;
    for (int i = 0 ; i < array.size() ; i++) {
        if (i > 0) string += ",";
        string += array[i];
    }
    return string;
}

/**
 * Self-centering within the parent.
 * I've done this for ConfigPanels forever but don't like it,
 * especially if you want this draggable.  How do you
 * center it when it is displayed for the first time but then
 * allowed to drag?  Can't do it in the constructor because we won't
 * necessarily have a parent then or the parent won't have a size yet.
 *
 * Feels like there should be something built-in for this.
 */
void JuceUtil::center(juce::Component* comp)
{
    int pwidth = comp->getParentWidth();
    int pheight = comp->getParentHeight();
    
    int mywidth = comp->getWidth();
    int myheight = comp->getHeight();
    
    if (mywidth > pwidth) mywidth = pwidth;
    if (myheight > pheight) myheight = pheight;

    int left = (pwidth - mywidth) / 2;
    int top = (pheight - myheight) / 2;
    
    comp->setTopLeftPosition(left, top);
}

//
// Newer centering tools

int JuceUtil::getCenterLeft(juce::Component* container, juce::Component* c)
{
    return (container->getWidth() / 2) - (c->getWidth() / 2);
}

int JuceUtil::getCenterTop(juce::Component* container, juce::Component* c)
{
    return (container->getHeight() / 2) - (c->getHeight() / 2);
}

void JuceUtil::centerInParent(juce::Component* c)
{
    juce::Component* parent = c->getParentComponent();
    c->setTopLeftPosition(getCenterLeft(parent, c), getCenterTop(parent, c));
}    
