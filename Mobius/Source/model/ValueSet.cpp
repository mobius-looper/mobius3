 
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../script/MslValue.h"
#include "ValueSet.h"

ValueSet::ValueSet()
{
}

ValueSet::~ValueSet()
{
}

ValueSet::ValueSet(ValueSet* src)
{
    name = src->name;
    scope = src->scope;

    juce::StringArray keys = src->getKeys();
    for (auto key : keys)
      set(key, *(src->get(key)));

    juce::OwnedArray<ValueSet>& subs = src->getSubsets();
    for (auto sub : subs)
      subsets.add(new ValueSet(sub));
}

juce::StringArray ValueSet::getKeys()
{
    juce::StringArray keys;
    for (juce::HashMap<String,MslValue*>::Iterator i(map) ; i.next();)
      keys.add(i.getKey());
    return keys;
}

juce::OwnedArray<ValueSet>& ValueSet::getSubsets()
{
    return subsets;
}

/**
 * Container get is simple enough.  Null means unbound.
 */
MslValue* ValueSet::get(juce::String key)
{
    return map[key];
}

/**
 * Set is a copy that may require storage allocation.
 * Lots of details to work out here but the audio thread should
 * only be modifying values that have been pre-allocated.
 * Might want a setSafely
 */
void ValueSet::set(juce::String key, MslValue& src)
{
    MslValue* value = map[key];
    if (value == nullptr)
      value = alloc(key);
    value->copy(&src);
}

MslValue* ValueSet::alloc(juce::String key)
{
    // should only be calling this if we no there isn't one, but make sure
    MslValue* current = map[key];
    if (current != nullptr) {
        Trace(1, "ValueSet: Not supposed to be replacing a value this way");
        values.removeObject(current);
    }
    MslValue* value = new MslValue();
    values.add(value);
    map.set(key, value);
    return value;
}

/**
 * Normally used only in cases where the ValueSet is being constructed
 * for the first time.  Ownership of the value transfers to the set.
 */
MslValue* ValueSet::replace(juce::String key, MslValue* value, bool deleteCurrent)
{
    MslValue* current = map[key];
    if (current != nullptr) {
        values.removeObject(current, false);
        if (deleteCurrent) {
            delete current;
            current = nullptr;
        }
    }
        
    map.set(key, value);
    values.add(value);
    
    return current;
}

/**
 * Various coercion accessors for convenience.
 */
const char* ValueSet::getString(juce::String key)
{
    const char* charval = nullptr;
    MslValue* value = map[key];
    if (value != nullptr)
      charval = value->getString();
    return charval;
}

void ValueSet::setString(juce::String key, const char* charval)
{
    MslValue* value = map[key];
    if (value == nullptr)
      value = alloc(key);
    value->setString(charval);
}

/**
 * For integers and booleans there is no "unbound" checking or default value.
 * The return value is zero.
 */
int ValueSet::getInt(juce::String key)
{
    int ival = 0;
    MslValue* value = map[key];
    if (value != nullptr)
      ival = value->getInt();
    return ival;
}

void ValueSet::setInt(juce::String key, int ival)
{
    MslValue* value = map[key];
    if (value == nullptr)
      value = alloc(key);
    value->setInt(ival);
}

bool ValueSet::getBool(juce::String key)
{
    bool bval = 0;
    MslValue* value = map[key];
    if (value != nullptr)
      bval = value->getBool();
    return bval;
}

void ValueSet::setBool(juce::String key, bool bval)
{
    MslValue* value = map[key];
    if (value == nullptr)
      value = alloc(key);
    value->setBool(bval);
}

//////////////////////////////////////////////////////////////////////
//
// Subsets
//
//////////////////////////////////////////////////////////////////////

/**
 * Return a subset for the given scope index.
 * Normaly these will be track numbers.
 * Should be fully fleshed out by the shell before the kernel needs to access them.
 */
ValueSet* ValueSet::getSubset(int index)
{
    ValueSet* sub = nullptr;
    if (index >= 0 && index < subsets.size())
      sub = subsets[index];
    return sub;
}

void ValueSet::addSubset(ValueSet* sub, int index)
{
    // the sparse array problem with Juce
    for (int i = subsets.size() ; i <= index ; i++)
      subsets.set(i, nullptr);

    subsets.set(index, sub);
}

//
// Name access
// I think this is better, reconsider indexed access
//

ValueSet* ValueSet::getSubset(juce::String setname)
{
    ValueSet* found = nullptr;
    for (auto sub : subsets) {
        if (sub->name == setname) {
            found = sub;
            break;
        }
    }
    return found;
}

/**
 * When adding a value set with a name, it replaces the
 * one with the previous name.
 */
ValueSet* ValueSet::addSubset(ValueSet* child)
{
    if (child->name.length() == 0) {
        // can't use this interface without a name?
        Trace(1, "ValueSet: Attempt to add ValueSet without a name");
    }
    else {
        int currentIndex = -1;
        for (int i = 0 ; i < subsets.size() ; i++) {
            ValueSet* sub = subsets[i];
            if (sub->name == child->name) {
                currentIndex = i;
                break;
            }
        }
        if (currentIndex >= 0) {
            subsets.set(currentIndex, child);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// XML
//
//////////////////////////////////////////////////////////////////////

/**
 * Value sets will normally be inside something.
 * todo: Any need to collapse these if they're empty?
 */
void ValueSet::render(juce::XmlElement* parent)
{
    juce::XmlElement* root = new juce::XmlElement("ValueSet");
    parent->addChildElement(root);

    if (name.length() > 0) 
      root->setAttribute("name", name);

    if (scope > 0)
      root->setAttribute("scope", scope);

    // XML serialization in HashMap::Iterator order is unstable across machines
    // which leads to xml file differences when they are under source control
    // while it isn't necessary for this to be stable in normal use, I hit
    // it all the time developing over several machines and it can make Git merges harder
    // will want to encapsuate this somewhere if we have more than one

    // first find and sort the keys
    juce::StringArray keys;
    for (juce::HashMap<juce::String,MslValue*>::Iterator i (map) ; i.next() ;) {
        keys.add(i.getKey());
    }
    keys.sort(false);
    // now emit the map in this order
    for (auto key : keys) {
        MslValue* value = map[key];
        // other property lists filter out null entries, is that relevant here?
        if (value != nullptr && !value->isNull()) {
            juce::XmlElement* valel = new juce::XmlElement("Value");
            root->addChildElement(valel);
            valel->setAttribute("name", key);
            valel->setAttribute("value", value->getString());

            // only expecting a few types and NO lists yet
            if (value->type == MslValue::Int) {
                valel->setAttribute("type", juce::String("int"));
            }
            else if (value->type == MslValue::Bool) {
                valel->setAttribute("type", juce::String("bool"));
            }
            else if (value->type == MslValue::Enum) {
                // should only see these from within the MSL interpreter
                // these are weird because they have both a string and an int representation
                // and they will be different
                valel->setAttribute("type", juce::String("enum"));
                valel->setAttribute("ordinal", juce::String(value->getInt()));
            }
            else if (value->type != MslValue::String) {
                // float, list, Symbol
                // shouldn't see these in a value set yet
                Trace(1, "ValueSet: Incompelte serialization of type %d", (int)value->type);
            }
        }
    }

    for (auto sub : subsets) {
        if (sub != nullptr)
          sub->render(root);
    }
}

/**
 * Caller is expecte to have identified the element <ValueSet>
 * and call here.
 */
void ValueSet::parse(juce::XmlElement* root)
{
    if (!root->hasTagName("ValueSet")) {
        Trace(1, "ValueSet: Asked to parse an element that was not ValueSet");
    }
    else {
        name = root->getStringAttribute("name");
        scope = root->getIntAttribute("scope");
        
        for (auto* el : root->getChildIterator()) {
            if (el->hasTagName("Value")) {
                juce::String key = el->getStringAttribute("name");
                MslValue* value = new MslValue();
                
                juce::String type = el->getStringAttribute("type");

                if (type.length() == 0) {
                    // string
                    juce::String jstr = el->getStringAttribute("value");
                    value->setString(jstr.toUTF8());
                }
                else if (type == "int") {
                    value->setInt(el->getIntAttribute("value"));
                }
                else if(type == "bool")  {
                    // el->getBoolAttribute should have the same rules as MslValue
                    // make sure of this! basically "true" and not "true"
                    value->setInt(el->getBoolAttribute("value"));
                }
                else if (type == "enum") {
                    // the weird one
                    juce::String jstr = el->getStringAttribute("value");
                    int ordinal = el->getIntAttribute("ordinal");
                    value->setEnum(jstr.toUTF8(), ordinal);
                }
                else {
                    // leave null
                    Trace(1, "ValueSet: Invalid value type %s", type.toUTF8());
                }
                
                MslValue* existing = replace(key, value);
                if (existing != nullptr) {
                    // must be parsing into an existin set, shouldn't happen
                    Trace(1, "ValueSet: Encountered existing value during parsing");
                    delete existing;
                }
            }
            else if (el->hasTagName("ValueSet")) {
                int trackNumber = el->getIntAttribute("scope");
                if (trackNumber > 0) {
                    ValueSet* sub = getSubset(trackNumber);
                    if (sub == nullptr) {
                        sub = new ValueSet();
                        sub->scope = trackNumber;
                        addSubset(sub, trackNumber);
                    }
                    sub->parse(el);
                }
                else {
                    // todo: not expecting unnumbered sub scopes yet,
                    // if we need to support this they must have a name
                    Trace(1, "ValueSet: Subscope without scope number");
                }
            }
            else {
                Trace(1, "ValueSet: Encountered invalid element %s", el->getTagName().toUTF8());
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
