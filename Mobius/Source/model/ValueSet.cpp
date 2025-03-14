 
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "../script/MslValue.h"

#include "Symbol.h"
#include "SymbolId.h"
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

    juce::StringArray keys;
    src->getKeys(keys);
    for (auto key : keys)
      set(key, *(src->get(key)));

    juce::OwnedArray<ValueSet>& subs = src->getSubsets();
    for (auto sub : subs)
      subsets.add(new ValueSet(sub));
}

/**
 * Only here for the copy constructor, could have a better way to do this.
 */
void ValueSet::getKeys(juce::StringArray& keys)
{
    for (juce::HashMap<juce::String,MslValue*>::Iterator i(map) ; i.next();)
      keys.add(i.getKey());
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
 * Parameter lookup using SymbolIds.  This is what most code
 * should use so we don't have to hard code names everywhere.
 */
MslValue* ValueSet::get(SymbolTable* symbols, SymbolId id)
{
    MslValue* value = nullptr;
    Symbol* s = symbols->getSymbol(id);
    if (s == nullptr) {
        Trace(1, "ValueSet: Invalid symbol id %d", id);
    }
    else {
        value = get(s->name);
    }
    return value;
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
    if (src.isNull()) {
        // setting something to null is the same as removing it
        // this is important for the Session/Overlay editors that may be
        // cleaning up invalid values, the way YanParameter works they will
        // often be left Null, rather than making everything deal with it
        // we can catch it consistently down here
        // this is a relatively new 3/8/25 change, watch for side effects
        if (value != nullptr)
          remove(key);
    }
    else {
        if (value == nullptr)
          value = alloc(key);
        value->copy(&src);
    }
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

void ValueSet::remove(juce::String key)
{
    MslValue* current = map[key];
    if (current != nullptr) {
        values.removeObject(current, true);
        map.remove(key);
    }
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

juce::String ValueSet::getJString(juce::String key)
{
    juce::String jval = "";
    MslValue* value = map[key];
    if (value != nullptr) {
        const char* charval = value->getString();
        if (charval != nullptr)
          jval = juce::String(charval);
    }
    return jval;
}

/**
 * 3/8/25 SessionEditor can sometimes deal with null or empty strings
 * which set() above considers a removal of the MslValue rather than
 * leaving it behind empty.
 * Start forwarding to that to handle it consistently
 */
void ValueSet::setString(juce::String key, const char* charval)
{
#if 0    
    MslValue* value = map[key];
    if (value == nullptr)
      value = alloc(key);
    value->setString(charval);
#endif
    MslValue v;
    v.setString(charval);
    set(key, v);
}

void ValueSet::setJString(juce::String key, juce::String jvalue)
{
#if 0    
    MslValue* value = map[key];
    if (value == nullptr)
      value = alloc(key);
    
    if (jvalue.length() > 0)
      value->setString(jvalue.toUTF8());
    else
      value->setNull();
#endif
    MslValue v;
    v.setJString(jvalue);
    set(key, v);
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

void ValueSet::clear()
{
    map.clear();
    values.clear();
    subsets.clear();
}

/**
 * Copy the values of one set into another.
 * This does NOT handle subsets, and the way the Session is shaking out
 * we're not going to be having subsets at all so those should be removed.
 */
void ValueSet::assimilate(ValueSet* src)
{
    // faster ways to do this, but it's an unusual operation
    if (src != nullptr) {
        juce::StringArray keys;
        src->getKeys(keys);
        for (auto key : keys) {
            MslValue* v = src->get(key);
            set(key, *v);
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Subsets
//
//////////////////////////////////////////////////////////////////////

/**
 * Nexted sets must have a unique name.
 * Adding one that already has that name replaces it.
 */
void ValueSet::addSubset(ValueSet* sub)
{
    if (sub->name.length() == 0) {
        Trace(1, "ValueSet: Can't add a subset without a reference name");
    }
    else {
        ValueSet* found = getSubset(sub->name);
        if (found != nullptr)
          subsets.removeObject(found);
        subsets.add(sub);
    }
}

/**
 * Obtain a subset by name.
 * Consider a HashMap index if there can be a lot of these.
 */
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
                    value->setBool(el->getBoolAttribute("value"));
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
                juce::String setname = el->getStringAttribute("name");
                if (setname.length() == 0) {
                    Trace(1, "ValueSet: Subset without name");
                }
                else {
                    ValueSet* sub = new ValueSet();
                    sub->parse(el);
                    subsets.add(sub);
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
