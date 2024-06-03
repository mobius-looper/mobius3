/*
 * Common base class for objects that define a fundamental
 * and relatively static part of the model.  These objects
 * have a unique internal name which is used in code and XML
 * configuration and usually have a "display name" that is
 * a modified version of the internal name suitable for display
 * in the UI.
 *
 * Examples include definitions for Functions and Parameters.
 * Configuration objects like MobiusConfig and Setup do not use
 * these since the names are user defined.
 *
 * Instances are almost always allocated using static initialization
 * and referenced with constant pointers such as SubcyclesParameter,
 * RecordFunction, etc.  They can have additional properties that
 * define behavior characteristics of the object like minimum and
 * maximum values, and hints for rendering it in the UI.
 *
 * Some serve more like simple enumerations but with the addition
 * of text that can be displayed when they need to be shown in the UI.
 *
 * In most cases there is a parallel model in the old code that
 * had a lot of additional data and methods used in the implementation.
 * I did not want to pollute the external model with those so when
 * necessary we map between them using "ordinals" which are numbers
 * assigned during construction and can be used as array indexes.
 * 
 * The old model this was derived from had the notion of a
 * "message catalog" for internationalization that is no longer used.
 * There is still support for this in the constructors in the
 * form of a "catalog key" as we transition old object definitions
 * away from catalogs.
 *
 * There was a placeholder for help text that could be shown in UI
 * tool tips that was never used but might be someday.
 *
 */

#pragma once

class SystemConstant {
  public:

    int ordinal = 0;
    const char* name = nullptr;
    const char* displayName = nullptr;

    // these are simple enough, could just use member initialization
    // SystemConstant();
	SystemConstant(const char* argName, const char* argDisplayName) {
        name = argName;
        displayName = argDisplayName;
    }

    // signature for the old core model that lost message keys
    // and doesn't have display names
	SystemConstant(const char* argName) {
        name = argName;
        // so we can call getDisplayName until we retool
        // everything not to expect a message catalog
        displayName = argName;
    }

    SystemConstant() {};

    // accessor for old code that still uses them
    const char* getName() {
        return name;
    }

    const char* getDisplayName() {
        return displayName;
    }

    const char* getDisplayableName() {
        return ((displayName != nullptr) ? displayName : name);
    }

    // old replicated Functions want to call these
    void setName(const char* argName) {
        name = argName;
    }

    void setDisplayName(const char* argName) {
        displayName = argName;
    }
    
    void setKey(int) {
        // if a message catalog falls in a forest?
    }

    // wow, Reset actually used this, was it a cry for help?
    void setHelp(const char*) {
    }
    
    virtual ~SystemConstant() {};

};
