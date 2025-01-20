/**
 * A model for symbol value bindings.
 * Eventual replacement for older parameter structures like Preset and Setup.
 *
 * The relationship between ValueSet, Symbol, and MSL needs a lot of thought an evolution.
 * There are overlapping concepts and it's hard to keep them independent.  The "no memory" rule
 * also makes this awkward.  juce::ValueTree uses a fixed Identifier rather than juce::String,
 * may want that.
 *
 * Conceptually similar to juce::ValueTree but I'm not sure I want to marry that yet.
 * Closely associated with the Symbol model.  I'm keeping value bindings off the Symbol
 * so they can be more easily modeled as autonomous collections and associated with Symbols
 * rather than living under them.
 *
 * Where it starts to differ from a map model or ValueTree is a more concrete notion of
 * nested value sets.  These represent value bindings for scopes within the application
 * that contain independent symbol bindings, for Mobius these are Tracks.
 *
 * The primary purpose of ValueSet is to represent bindings to Symbols that are associated
 * with Mobius parameters.  But it should be usable for generic name/value pairs in other contexts.
 * 
 * Value sets may have names so they can be managed in a UI as named entities.
 * Consider wrapping this in a ParameterSet or something more specific if this starts
 * growing things other than a name.
 *
 * Values within the set need to allow a few data types, similar to juce::var.  Since these
 * will be closely related to the MSL scriping language, MslValue is used but these will
 * not be pooled within the MslEnvironment.  I'd like to keep MSL completely independent so
 * the dependency is from ValueSet to MslValue rather than to from the MSL data model
 * to something external to it.
 *
 * Use of MslValue is also of interest to avoid potential memory allocation when accessed
 * from within the audio thread.  
 *
 * May need to wrap MslValue in another layer to quickly model the Symbol/Value associationl.
 *
 *    class SymbolBinding {
 *       Symbol* symbol;
 *       MslValue* value;
 *    }
 *
 * ValueSets have an XML serialization:
 *
 *    <ValueSet name='Global'
 *       <Value name='aString' value='foo'/>
 *       <Value name='aNumber' value='42' type='int'/>
 *       <Value name='switchQuantize' value='confirm' type='enum' ordinal='4'/>
 *
 */

#pragma once

// model for values
#include "../script/MslValue.h"
#include "SymbolId.h"

class ValueSet
{
  public:

    ValueSet();
    ~ValueSet();
    ValueSet(ValueSet* src);
    
    /**
     * Sets may have a name for management in a UI
     */
    juce::String name;

    // standard set names, move these outside?
    const char* GlobalSet = "Global";

    //
    // Various forms of value access
    //

    /**
     * Not using pass by value here though that wouldn't be too bad since
     * most values are integers or enum ordinals.  Reconsider.
     */
    MslValue* get(juce::String name);

    /**
     * The most common for accessing parameters by id.
     */
    MslValue* get(class SymbolTable* symbols, SymbolId id);

    /**
     * Clear the table
     */
    void clear();

    /**
     * Set is always by a copy of the source object to the internal object.
     * todo: If the value does not exist within the set, one will be created with "new".
     * If this can happen while in the audio thread will need to provide a pool or require
     * using replace() and have the caller return the current object (which usually won't exist)
     * to a pool.
     *
     * Rethink use of juce::String here, are those always guaranteed to be stack allocated?
     */
    void set(juce::String name, MslValue& src);

    /**
     * Replace a value with another.  Return original value if any so it can
     * be freed to a pool.
     */
    MslValue* replace(juce::String name, MslValue* src, bool deleteCurrent=false);

    const char* getString(juce::String name);
    void setString(juce::String name, const char* value);
    juce::String getJString(juce::String name);
    void setJString(juce::String name, juce::String value);
    
    int getInt(juce::String name);
    void setInt(juce::String name, int ival);

    bool getBool(juce::String name);
    void setBool(juce::String name, bool bval);

    juce::StringArray getKeys();

    //
    // Subset access
    //

    ValueSet* getSubset(juce::String name);
    void addSubset(ValueSet* sub);

    juce::OwnedArray<ValueSet>& getSubsets();

    //
    // XML
    //

    constexpr static const char* XmlElement = "ValueSet";

    void render(juce::XmlElement* parent);
    void parse(juce::XmlElement* root);
        
  private:

    // the name/value mapping structure
    juce::HashMap<juce::String,MslValue*> map;

    // the official owner for MslValues for deletion
    juce::OwnedArray<MslValue> values;

    // nested value scopes
    juce::OwnedArray<ValueSet> subsets;

    MslValue* alloc(juce::String name);
};

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
