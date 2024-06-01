/**
 * Experimental definitions for user-defined variables.
 * These will become Symbols with BehaviorVariable
 *
 * Do not confuse these with UserVariable which is old and will
 * be phased out.
 *
 * Conceptually a Variable is a named value whose name is defined
 * by a VariableDefinition and interned as a Symbol.
 *
 * The value of a variable is contained in an abstract value container
 * that may have several implementations.  There are roughly two categories
 * of value containers, legacy objects such as a Preset accessed through
 * a UIParameter, and a new VariableStore which is in the process of being defined.
 *
 * The VariableDefinition contains a set of "properties" that define characteristics
 * of both variable and the values it can contain.
 *
 * So allow for easier extensibility properties are implemented as a name/value "map"
 * whose implementation is not defined but will be some evolving use of juce features
 * such as HashMap and ValueTree.
 *
 */

#pragma once

class VariableDefinition
{
    friend class VariableDefinitionSet;
    
  public:

    VariableDefinition();
    ~VariableDefinition();

    juce::String name;

    // todo: should we have a level here?

    // properties
    juce::var get(juce::String propertyName);
    void set(juce::String propertyName, juce::var value);

    int getInt(juce::String name);
    bool getBool(juce::String name);
    float getFloat(juce::String name);
    juce::String getString(juce::String name);

    // XML
    constexpr static const char* Element = "VariableDefinition";
    constexpr static const char* Property = "Property";
    constexpr static const char* AttName = "name";
    constexpr static const char* AttValue = "value";
    // ...
    
  protected:
    
    // can use the old XML system for now, but it should be opaque so
    // we can switch to juce::XmlElement later
    void render(class XmlBuffer* b);
    void parse(class XmlElement* e);

  private:

    juce::HashMap<juce::String,juce::var> properties;

};

class VariableDefinitionSet
{
  public:

    VariableDefinitionSet();
    ~VariableDefinitionSet();
    
    juce::OwnedArray<VariableDefinition> variables;

    constexpr static const char* Element = "VariableDefinitionSet";
    juce::String toXml();
    void parseXml(juce::String xml);
    
  protected:

    void render(class XmlBuffer* b);
    void parse(class XmlElement* e);
    
  private:
    
};



/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

