/**
 * Base class for a named object within the configuration model.
 * 
 * Structures are logically collections of arbitrary values that
 * define characteristics of runtime or display behavior.
 *
 * This interface provides a way for them to be dealt with generically
 * in the configuration user interface, and to be used in Bindings
 * with the ActionActivation action type to select them for use.
 *
 * Some characteristics of a Structure
 *
 *   - has a unique name within a structure class (Preset, Setup, BindingSet)
 *   - has a unique zero based ordinal number for internal references
 *   - is usually maintained in a collection and provides an interface
 *     for iterating over that collection
 *   - provides an interface for cloning
 *
 * These have historically implemented collections as a linked list
 * with an internal pointer.  I'm keeping that for now to prevent code
 * disruption, but this really needs to be migrated to a std::vector
 * or juce::OwnedArray which would be easier, but I don't have time
 * right now to work through the polymorphism issues.
 *
 * I'm also keeping names as const char* rather than juce::String
 * for consistency with the rest of the older model, but need to revisit that.
 *
 */

#pragma once

class Structure
{
  public:

    Structure();
    virtual ~Structure();

    int ordinal;
    
    const char* getName();
    void setName(const char* name);

    Structure* getNext();
    void setNext(Structure* next);

    // this must be implemented by the subclass
    // make a complete copy without the chain pointer
    virtual Structure* clone() = 0;

    static int count(Structure *list);
    static Structure* find(Structure* list, const char* name);
    static void ordinate(Structure* list);
    static Structure* append(Structure* list, Structure* neu);
    
    static int getOrdinal(Structure* list, const char* name);
    static Structure* get(Structure* list, int ordinal);
        
  private:

    Structure* mNext;
    char* mName;
};

