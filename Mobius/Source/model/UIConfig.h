/*
 * Model for user interface configuration.
 *
 * The root object is UIConfig which has a few UI system-wide preferences
 * such window size, message timeouts, and graphics options.
 *
 * The Mobius UI is divided into these major sections:
 *
 *     - main menu bar
 *     - action buttons
 *     - status area
 *     - track strips
 *
 * The main menu bar at the top can be turned on and off but the items cannot be
 * customized.
 *
 * Most of the display is occupied by the status area and the track strips.  These contain
 * a number of Display Elements which show the current state of the application.  Each display
 * element can be turned on and off, moved to different locations, and resized.
 *
 * The bulk of the UIConfig ia contained in a Layout object.  A Layout defines which
 * display elements are displayed and where they are located.  The Layout also contains
 * the definition for two track strips, a set of 8 "docked" strips which are shown at the
 * bottom of the window and one "floating" strip that can be moved around in the status
 * area.
 *
 */

#pragma once

#include <JuceHeader.h>

/**
 * Defines a single element that may be included in the display.
 * There will be a set of instances of this class contained in the UIConfig,
 * they define basic information about the elements that CAN be addedd to the
 * display and how they behave.  This set of objects is built-in to the system
 * and cannot be changed.
 *
 * The DisplayElement class defines the properties of a concrete use of
 * this element, including where it is, size preferences, color preferences, etc.
 */
class DisplayElementDefinition
{
  public:

    DisplayElementDefinition() {}
    DisplayElementDefinition(const char* argName, bool isTrackStrip = false) {
        name = argName;
        statusArea = !isTrackStrip;
        trackStrip = isTrackStrip;
    }
    ~DisplayElementDefinition() {}

    /**
     * Internal name used in code.
     */
    juce::String name;

    /**
     * Text shown when configuring then in the UI.
     */
    juce::String displayName;

    /**
     * True if this element is limited to the static or floating track strips.
     * todo: temporary as I'd like to allow any element type to be available
     * everywhere.
     */
    bool trackStrip = false;

    /**
     * True if this element is limited to the main display area.
     */
    bool statusArea = false;

    juce::String toXml();
    void parseXml(class XmlElement* e);
    
};

/**
 * Defines the instance of a display element in a layout and it's location
 * within the status area.  DisplayElements are not normally removed from a layout
 * they are simply disablled, so they can be invisible but still retain their former
 * location.
 */
class DisplayElement
{
  public:

    DisplayElement() {};
    DisplayElement(DisplayElement* src);
    ~DisplayElement() {};

    // The name of the DisplayElemntDefinition that controls the appearance and behavior
    // of this element
    juce::String type;

    // Optional user defined name.  Most elements will not have names, since there
    // can only be one of them and they are identified by their type
    // in the future, "container" types will exist that allow for more than one instance
    // and these are given unique names, for example several floating track strips
    juce::String name;

    // location within the status area if this isn't in a track strip
    int x = 0;
    int y = 0;

    // size if adjusted from the default
    int width = 0;
    int height = 0;

    // true if this element is disabled
    // disabled elements will not be visible, but will retain their location and size
    // so they can be restored if desired
    bool disabled = false;
};

/**
 * A container of DisplayElements that are oragnized as a unit.
 * When a strip is moved or sized, all elements within it are also changed.
 * Strips enforce a visual organization of their contained elements, usually
 * vertical or horizontal.
 */
class DisplayStrip
{
  public:

    DisplayStrip() {}
    DisplayStrip(DisplayStrip* src);
    ~DisplayStrip() {}

    /**
     * The reserved name of the DisplayStrip used to configure the docked
     * track strips at the bottom.
     */
    constexpr static const char* Docked = "Docked";

    /**
     * The reserved name of the DisplayStrip that floats within the status area.
     */
    constexpr static const char* Floating = "Floating";

    // when there is more than one strip in the status area, each will have a unique name
    juce::String name;

    // elements within the strip
    juce::OwnedArray<DisplayElement> elements;

};

/**
 * A layout is a full set of UI elements that includes;
 *    - status area elements in the center
 *    - track strip elements at the bottom
 *
 * There can be multiple Layouts in the UIConfig identified by name and selected
 * at runtime by the user.
 */
class DisplayLayout
{
  public:

    DisplayLayout() {}
    DisplayLayout(DisplayLayout* src);
    ~DisplayLayout() {}

    juce::String name;

    // elements in the main status area
    juce::OwnedArray<DisplayElement> mainElements;

    // docked and floaging strips
    juce::OwnedArray<DisplayStrip> strips;

    //
    // Element specific preferences
    // todo: This is temporary, as we add more elements with configurable
    // preferences, these should go inside each DisplayElement.  The only
    // one we have right now is the "Instant Parameters" element which
    // displays a list of the active parameter values in a track
    // 
    
    juce::StringArray instantParameters;

    DisplayStrip* getDockedStrip();
    DisplayStrip* getFloatingStrip();
    DisplayStrip* findStrip(juce::String name, bool create = false);

    DisplayElement* getElement(juce::String name);
    
};

/**
 * A DisplayButton is a special kind of element that is always displayed avove
 * the status area.  Buttons do not display runtime state, they cause the
 * execution of "actions" when they are clicked.  An action can include
 * a looping function such as Record, setting a parameter value, running a script,
 * or activating a preset.
 */
class DisplayButton
{
  public:

    DisplayButton() {}
    ~DisplayButton() {}

    // A name defining what this button does
    // Internally this will be the name of a Symbol which is associated
    // with Functions, Parameters, or Strucdtures
    juce::String action;

    // optional arguments that may be applied to the action
    // the format of this will depend on the chosen action
    juce::String arguments;

    // option scope identifier that restricts this action to one or more tracks
    // the value may be a track number or a group letter
    // if not specified the scope is Global and applies to all tracks with focus
    juce::String scope;

    // user defined name for button, shown within the graphics for the button
    // this is optional, if not set it will be the symbol name
    juce::String name;

    // alternate color
    int color = 0;

    // kludge: transient id number used to correlate this with a Binding
    // when editing in the ButtonPanel
    int id = 0;

};

/**
 * A ButtonSet set is a collection of DisplayButtons.  The user may define
 * multiple button sets and swap them in and out of the UI at runtime.  You
 * can think of them like "banks" on a MIDI controller.
 */
class ButtonSet
{
  public:

    ButtonSet() {}
    ButtonSet(ButtonSet* src);
    ~ButtonSet() {}

    // name used to select this button set
    juce::String name;

    // the buttons that will be displayed when this set is active
    juce::OwnedArray<DisplayButton> buttons;
    
    DisplayButton* getButton(juce::String id);
    DisplayButton* getButton(DisplayButton* src);
};

/**
 * The UIConfig is the root object that wraps all the other display-related
 * objects and is stored in the uiconfig.xml file.
 */
class UIConfig
{
  public:

    UIConfig();
    ~UIConfig();

    // size of the outer window
    int windowWidth = 0;
    int windowHeight = 0;

    // the definitions of the elements that can be displayed
    juce::OwnedArray<DisplayElementDefinition> definitions;

    /**
     * This defines a subset of all possible parameter symbols
     * that may be allowed for selection, it isn't necessary but
     * there is soo much crap in the full list that people aren't
     * going to be interested in, restricting the list makes it
     * easier to use.
     */
    juce::StringArray availableParameters;
    
    // the layouts of elements that may be selected
    juce::OwnedArray<DisplayLayout> layouts;
    juce::String activeLayout;

    // the button sets that may be selected
    juce::OwnedArray<ButtonSet> buttonSets;
    juce::String activeButtonSet;

    // flag set to enable StatusArea borders and titles for arrangement
    bool showBorders = false;

    /**
     * Arbitrary estensible properties.
     * Consider moving showBorders in here now
     */
    juce::HashMap<juce::String,juce::String> properties;

    // flag set whenever this is modified at runtime
    // used during Supervisor shutdown to write the changes to the file
    // reset whenever the file is written
    // since we don't wrap the variables in accessor functions we have
    // to rely on the kindness of strangers to set this when appropriate
    bool dirty = false;
    
    //
    // Runtime methods
    //

    // find one of the element definitiosn by name
    DisplayElementDefinition* findDefinition(juce::String name);

    // find layouts
    // This is technically a "Structure" but we're implementing the
    // collection with an OwnedArray rather than the old linked list convention
    // so none of the static Structure utilities work here
    DisplayLayout* findLayout(juce::String name);
    int getLayoutOrdinal(juce::String name);
    DisplayLayout* getActiveLayout();

    // find button sets
    ButtonSet* findButtonSet(juce::String name);
    int getButtonSetOrdinal(juce::String name);
    ButtonSet* getActiveButtonSet();


    // type casing property getters
    int getInt(juce::String name);
    bool getBool(juce::String name);
    juce::String get(juce::String name);
    
    void put(juce::String name, juce::String value);
    void putInt(juce::String name, int value);
    void putBool(juce::String name, bool value);

    // Build the UIConfig from XML
    void parseXml(juce::String xml);
    juce::String toXml();
    
  private:

    // Build the DisplayElementDefinition list in code rather than
    // reading it from a file.  Since these can't be user modified
    // we don't realy need file storage for them
    void hackDefinitions();

    // internal XML parsing and rendering tools
    DisplayLayout* parseLayout(juce::XmlElement* root);
    DisplayElement* parseElement(juce::XmlElement* root);
    DisplayStrip* parseStrip(juce::XmlElement* root);
    ButtonSet* parseButtonSet(juce::XmlElement* root);
    DisplayButton* parseButton(juce::XmlElement* root);
    void parseProperties(juce::XmlElement* root, juce::HashMap<juce::String,juce::String>& map);
    void xmlError(const char* msg, juce::String arg);

    void renderLayout(juce::XmlElement* parent, DisplayLayout* layout);
    void renderElement(juce::XmlElement* parent, DisplayElement* el);
    void renderStrip(juce::XmlElement* parent, DisplayStrip* strip);
    void renderButtonSet(juce::XmlElement* parent, ButtonSet* set);
    void renderButton(juce::XmlElement* parent, DisplayButton* button);
    void renderProperties(juce::XmlElement* parent, juce::HashMap<juce::String, juce::String>& props);
    
};
