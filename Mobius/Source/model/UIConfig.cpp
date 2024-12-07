
#include <JuceHeader.h>

#include "../util/Trace.h"
#include "UIConfig.h"
#include "MobiusConfig.h"
#include "UIAction.h"
#include "Scope.h"
#include "Symbol.h"
#include "Binding.h"

#include "UIConfig.h"

//////////////////////////////////////////////////////////////////////
//
// UIElementDefinition
//
//////////////////////////////////////////////////////////////////////

UIElementDefinition::UIElementDefinition()
{
}

UIElementDefinition::UIElementDefinition(const char* argName, bool isTrackStrip)
{
    name = argName;
    statusArea = !isTrackStrip;
    trackStrip = isTrackStrip;
    intrinsic = true;
}

UIElementDefinition::~UIElementDefinition()
{
}

void UIConfig::renderDefinition(juce::XmlElement* parent, UIElementDefinition* def)
{
    // suppress the intrinsics
    if (!def->intrinsic) {
        juce::XmlElement* root = new juce::XmlElement("ElementDefinition");
        parent->addChildElement(root);

        if (def->name.length() > 0) root->setAttribute("name", def->name);
        if (def->displayName.length() > 0) root->setAttribute("displayName", def->displayName);
        if (def->visualizer.length() > 0) root->setAttribute("visualizer", def->visualizer);

        // todo: convert the old model flags into areas
        juce::StringArray areas;
        if (def->trackStrip)
          areas.add("strip");
        if (def->statusArea)
          areas.add("main");
        juce::String area = areas.joinIntoString(",");
        if (area.length() > 0) root->setAttribute("area", area);

        renderProperties(root, def->properties);
    }
}

UIElementDefinition* UIConfig::parseDefinition(juce::XmlElement* root)
{
    UIElementDefinition* def = new UIElementDefinition();

    def->name = root->getStringAttribute("name");
    def->displayName = root->getStringAttribute("displayName");
    def->visualizer = root->getStringAttribute("visualizer");

    juce::String area = root->getStringAttribute("area");
    if (area.length() > 0) {
        juce::StringArray areas = juce::StringArray::fromTokens(area, ",", "");
        def->statusArea = areas.contains("main");
        def->trackStrip = areas.contains("strip");
    }
    else {
        // for initial testing, the absense of an area implies both
        def->statusArea = true;
        def->trackStrip = true;
    }
    
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Properties")) {
            def->properties.clear();
            parseProperties(el, def->properties);
        }
        else {
            xmlError("Unexpected XML tag name: %s\n", el->getTagName());
        }
    }

    return def;
}

// Searching these object lists doesn't happen often so we'll
// just do linear for simplicity

UIElementDefinition* UIConfig::findDefinition(juce::String name)
{
    UIElementDefinition* found = nullptr;
    for (auto def : definitions) {
        if (def->name == name) {
            found = def;
            break;
        }
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// UIConfig
//
//////////////////////////////////////////////////////////////////////

UIConfig::UIConfig()
{
}

UIConfig::~UIConfig()
{
}

DisplayLayout* UIConfig::findLayout(juce::String name)
{
    DisplayLayout* found = nullptr;
    for (auto layout : layouts) {
        if (layout->name == name) {
            found = layout;
            break;
        }
    }
    return found;
}

ButtonSet* UIConfig::findButtonSet(juce::String name)
{
    ButtonSet* found = nullptr;
    for (auto set : buttonSets) {
        if (set->name == name) {
            found = set;
            break;
        }
    }
    return found;
}

DisplayLayout* UIConfig::getActiveLayout()
{
    DisplayLayout* active = findLayout(activeLayout);
    if (active == nullptr) {
        // invalid or missing name, used the first one
        if (layouts.size() == 0) {
            // synthesize one
            active = new DisplayLayout();
            active->name = "New";
            layouts.add(active);
        }
        else {
            active = layouts[0];
        }
    }
    return active;
}

ButtonSet* UIConfig::getActiveButtonSet()
{
    ButtonSet* active = findButtonSet(activeButtonSet);
    if (active == nullptr) {
        // invalid or missing name, used the first one
        if (buttonSets.size() == 0) {
            // synthesize one
            active = new ButtonSet();
            active->name = "New";
            buttonSets.add(active);
        }
        else {
            active = buttonSets[0];
        }
    }
    return active;
}

/**
 * Return the "ordinal" for theh layout which is the index
 * into the layout array.
 *
 * This is used to select layouts from continuous controllers
 * rather than by name.
 */
int UIConfig::getLayoutOrdinal(juce::String name)
{
    int ordinal = -1;
    int index = 0;
    for (auto layout : layouts) {
        if (layout->name == name) {
            ordinal = index;
            break;
        }
        index++;
    }
    return ordinal;
}


int UIConfig::getButtonSetOrdinal(juce::String name)
{
    int ordinal = -1;
    int index = 0;
    for (auto set : buttonSets) {
        if (set->name == name) {
            ordinal = index;
            break;
        }
        index++;
    }
    return ordinal;
}

/**
 * True if this binding set is active.
 * Handles both the single alternate binding and
 * any number of overlay bindings.
 */
bool UIConfig::isActiveBindingSet(juce::String name)
{
    return (name == activeBindings || activeOverlays.contains(name));
}

//
// Properties
//

void UIConfig::put(juce::String name, juce::String value)
{
    properties.set(name, value);
}

void UIConfig::putInt(juce::String name, int value)
{
    properties.set(name, juce::String(value));
}

void UIConfig::putBool(juce::String name, bool value)
{
    properties.set(name, (value) ? juce::String("true") : juce::String("false"));
}

juce::String UIConfig::get(juce::String name)
{
    return properties[name];
}

int UIConfig::getInt(juce::String name)
{
    juce::String value = properties[name];
    return value.getIntValue();
}

bool UIConfig::getBool(juce::String name)
{
    juce::String value = properties[name];
    return (value == "true") ? true : false;
}

//////////////////////////////////////////////////////////////////////
//
// Locations
//
//////////////////////////////////////////////////////////////////////

UILocation::UILocation(juce::String csv)
{
    fromCsv(csv);
}

UILocation::UILocation(juce::Component* c)
{
    if (c != nullptr) {
        x = c->getX();
        y = c->getY();
        width = c->getWidth();
        height = c->getHeight();
    }
}

juce::String UILocation::toCsv()
{
    juce::StringArray a;
    a.add(juce::String(x));
    a.add(juce::String(y));
    a.add(juce::String(width));
    a.add(juce::String(height));
    return a.joinIntoString(",");
}

void UILocation::fromCsv(juce::String csv)
{
    x = 0;
    y = 0;
    width = 0;
    height = 0;
    
    juce::StringArray list = juce::StringArray::fromTokens(csv, ",", "");
    if (list.size() == 4) {
        x = list[0].getIntValue();
        y = list[1].getIntValue();
        width = list[2].getIntValue();
        height = list[3].getIntValue();
    }
    else if (list.size() > 0) {
        Trace(1, "UIConfig: Malformed location string %s", csv.toUTF8());
    }
}

/**
 * Carefully adjust bounds for a component.  Only set the fields
 * we know are interesting.
 */
void UILocation::adjustBounds(juce::Rectangle<int>& bounds)
{
    if (x > 0) bounds.setX(x);
    if (y > 0) bounds.setY(y);
    if (width > 0) bounds.setWidth(width);
    if (height > 0) bounds.setHeight(height);
}
    
/**
 * We started storing just the width and height and the window
 * was centered on the display.  Unclear how much control over plugin
 * editor windows there was.
 */
UILocation UIConfig::getWindowLocation()
{
    UILocation loc;
    loc.x = 0;
    loc.y = 0;
    loc.width = windowWidth;
    loc.height = windowHeight;
    return loc;
}

/**
 * This one is stored in a new way in properties as a csv which
 * is easier.
 */
UILocation UIConfig::getScriptWindowLocation()
{
    return UILocation(get("scriptWindow"));
}

void UIConfig::captureLocations(juce::Component* main, juce::Component* script)
{
    // todo: use the old properties for awhile
    windowWidth = main->getWidth();
    windowHeight = main->getHeight();

    // ! ahh the problem here is that MainWindow is not a DocumentWindow
    // so the origin is always 0,0, need to locate the containing root window
    // this might be awkward with plugineditors
    put("mainWindow", UILocation(main).toCsv());

    // this is commonly not open so don't reset to zero
    if (script != nullptr)
      put("scriptWindow", UILocation(script).toCsv());
}

//////////////////////////////////////////////////////////////////////
//
// DisplayLayout
//
//////////////////////////////////////////////////////////////////////

DisplayLayout::DisplayLayout(DisplayLayout* src)
{
    name = src->name;

    for (auto el : src->mainElements)
      mainElements.add(new DisplayElement(el));

    for (auto strip : src->strips)
      strips.add(new DisplayStrip(strip));

    instantParameters = src->instantParameters;
}

DisplayElement::DisplayElement(DisplayElement* src)
{
    type = src->type;
    name = src->name;
    x = src->x;
    y = src->y;
    width = src->width;
    height = src->height;
    // for editing, this probably doesn't need to be copied
    disabled = src->disabled;
}

DisplayStrip::DisplayStrip(DisplayStrip* src)
{
    name = src->name;

    for (auto el : src->elements)
      elements.add(new DisplayElement(el));
}

DisplayStrip* DisplayLayout::getDockedStrip()
{
    return findStrip(DisplayStrip::Docked, true);
}

DisplayStrip* DisplayLayout::getFloatingStrip()
{
    return findStrip(DisplayStrip::Floating, true);
}

/**
 * Find a DisplayStrip, bootstrapping one if it does not exist.
 */
DisplayStrip* DisplayLayout::findStrip(juce::String stripName, bool create)
{
    DisplayStrip* found = nullptr;
    for (auto strip : strips) {
        if (strip->name == stripName) {
            found = strip;
            break;
        }
    }
    if (found == nullptr && create) {
        found = new DisplayStrip();
        found->name = stripName;
        strips.add(found);
    }
    return found;
}

DisplayElement* DisplayLayout::getElement(juce::String elementName)
{
    // do this a lot, considier factoring out a "named thing" superclass
    DisplayElement* found = nullptr;
    for (auto element : mainElements) {
        if (element->name == elementName) {
            found = element;
            break;
        }
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// ButtonSet
//
//////////////////////////////////////////////////////////////////////

/**
 * Really need to figure out how compiler generated copy constructors
 * would work here with OwnedArrays.
 */
ButtonSet::ButtonSet(ButtonSet* src)
{
    name = src->name;
    for (auto srcButton : src->buttons) {
        DisplayButton* neu = new DisplayButton();
        neu->action = srcButton->action;
        neu->arguments = srcButton->arguments;
        neu->scope = srcButton->scope;
        neu->name = srcButton->name;
        neu->color = srcButton->color;
        buttons.add(neu);
    }
}

/**
 * Find a DisplayButton matching the action name, scope, and arguments.
 * The combination of those three is the only way to uniquely idenftify a button.
 * Used when we need to save edited state for an ActionButton.
 */
DisplayButton* ButtonSet::getButton(juce::String action, juce::String scope, juce::String args)
{
    DisplayButton* found = nullptr;
    for (auto button : buttons) {
        if (button->action == action &&
            button->scope == scope &&
            button->arguments == args) {
            found = button;
            break;
        }
    }
    return found;
}

/**
 * Added for UpgradePanel
 * Look for a matching button definition including the name,
 * arguments, and scope.
 */
DisplayButton* ButtonSet::getButton(DisplayButton* src)
{
    DisplayButton* found = nullptr;
    if (src != nullptr)
      found = getButton(src->action, src->scope, src->arguments);
    return found;
}

/**
 * Searcher the ActionButton popup that deals with UIActions.
 */
DisplayButton* ButtonSet::getButton(UIAction* action)
{
    DisplayButton* found = nullptr;
    if (action != nullptr && action->symbol != nullptr) {
        const char* scope = action->getScope();
        found = getButton(action->symbol->name, juce::String(scope), juce::String(action->arguments));
    }
    return found;
}

/**
 * Searcher the ActionButton popup that deals with Binding.
 */
DisplayButton* ButtonSet::getButton(Binding* binding)
{
    DisplayButton* found = nullptr;
    if (binding != nullptr) {
        const char* symbolName = binding->getSymbolName();
        // can be null for new empty bindings
        if (symbolName != nullptr) {
            const char* scope = binding->getScope();
            found = getButton(juce::String(symbolName), juce::String(scope), juce::String(binding->getArguments()));
        }
    }
    return found;
}

//////////////////////////////////////////////////////////////////////
//
// XML Parsing
//
//////////////////////////////////////////////////////////////////////

void UIConfig::parseXml(juce::String xml)
{
    juce::XmlDocument doc(xml);
    std::unique_ptr<juce::XmlElement> root = doc.getDocumentElement();
    if (root == nullptr) {
        xmlError("Parse parse error: %s\n", doc.getLastParseError());
    }
    else if (!root->hasTagName("UIConfig")) {
        xmlError("Unexpected XML tag name: %s\n", root->getTagName());
    }
    else {
        windowWidth = root->getIntAttribute("windowWidth");
        windowHeight = root->getIntAttribute("windowHeight");
        
        activeButtonSet = root->getStringAttribute("activeButtonSet");
        activeLayout = root->getStringAttribute("activeLayout");
        activeBindings = root->getStringAttribute("activeBindings");
        juce::String csv = root->getStringAttribute("activeOverlays");
        activeOverlays = juce::StringArray::fromTokens(csv, ",", "");
        
        for (auto* el : root->getChildIterator()) {
            if (el->hasTagName("Layout")) {
                layouts.add(parseLayout(el));
            }
            else if (el->hasTagName("ButtonSet")) {
                buttonSets.add(parseButtonSet(el));
            }
            else if (el->hasTagName("Properties")) {
                properties.clear();
                parseProperties(el, properties);
            }
            else if (el->hasTagName("ElementDefinition")) {
                definitions.add(parseDefinition(el));
            }
            else {
                xmlError("Unexpected XML tag name: %s\n", el->getTagName());
            }
        }

        // these are not serialized but the configuration UI needs to see them
        hackDefinitions();
    }
}

DisplayLayout* UIConfig::parseLayout(juce::XmlElement* root)
{
    DisplayLayout* layout = new DisplayLayout();

    layout->name = root->getStringAttribute("name");
    
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Element")) {
            layout->mainElements.add(parseElement(el));
        }
        else if (el->hasTagName("Strip")) {
            layout->strips.add(parseStrip(el));
        }
        else if (el->hasTagName("InstantParameters")) {
            // don't like the model here, this should be inside
            // the DisplayElement with type ParametersElement
            juce::String csv = el->getStringAttribute("names");
            layout->instantParameters = juce::StringArray::fromTokens(csv, ",", "");
        }
    }
    return layout;
}

DisplayElement* UIConfig::parseElement(juce::XmlElement* root)
{
    DisplayElement* del = new DisplayElement();

    del->type = root->getStringAttribute("type");
    del->name = root->getStringAttribute("name");
    del->x = root->getIntAttribute("x");
    del->y = root->getIntAttribute("y");
    del->width = root->getIntAttribute("width");
    del->height = root->getIntAttribute("height");
    del->disabled = root->getBoolAttribute("disabled");
    
    return del;
}

DisplayStrip* UIConfig::parseStrip(juce::XmlElement* root)
{
    DisplayStrip* strip = new DisplayStrip();

    strip->name = root->getStringAttribute("name");
    
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Element")) {
            strip->elements.add(parseElement(el));
        }
    }
    return strip;
}

ButtonSet* UIConfig::parseButtonSet(juce::XmlElement* root)
{
    ButtonSet* set = new ButtonSet();

    set->name = root->getStringAttribute("name");
    
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Button")) {
            set->buttons.add(parseButton(el));
        }
    }
    return set;
}

DisplayButton* UIConfig::parseButton(juce::XmlElement* root)
{
    DisplayButton* button = new DisplayButton();
    button->action = root->getStringAttribute("action");
    button->arguments = root->getStringAttribute("arguments");
    button->scope = root->getStringAttribute("scope");
    button->name = root->getStringAttribute("name");
    button->color = root->getIntAttribute("color");
    return button;
}

void UIConfig::parseProperties(juce::XmlElement* root, juce::HashMap<juce::String,juce::String>& map)
{
    for (auto* el : root->getChildIterator()) {
        if (el->hasTagName("Property")) {
            juce::String key = el->getStringAttribute("name");
            juce::String value = el->getStringAttribute("value");
            if (key.length() > 0) {
                map.set(key, value);
            }
        }
    }
}

void UIConfig::xmlError(const char* msg, juce::String arg)
{
    juce::String fullmsg ("UIConfig: " + juce::String(msg));
    if (arg.length() == 0)
      Trace(1, fullmsg.toUTF8());
    else
      Trace(1, fullmsg.toUTF8(), arg.toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// XML Rendering
//
//////////////////////////////////////////////////////////////////////

juce::String UIConfig::toXml()
{
    juce::XmlElement root ("UIConfig");

    // definitions don't need to be serialized yet, we will
    // generate them at runtime, same with availableParameters

    if (windowWidth > 0)
      root.setAttribute("windowWidth", windowWidth);
    
    if (windowHeight > 0)
      root.setAttribute("windowHeight", windowHeight);

    // could fix these if they're stale
    if (activeButtonSet.length() > 0) 
      root.setAttribute("activeButtonSet", activeButtonSet);

    if (activeLayout.length() > 0)
      root.setAttribute("activeLayout", activeLayout);
    
    if (activeBindings.length() > 0)
      root.setAttribute("activeBindings", activeBindings);

    if (activeOverlays.size() > 0)
      root.setAttribute("activeOverlays", activeOverlays.joinIntoString(","));
    
    if (showBorders)
      root.setAttribute("showBorders", showBorders);

    for (auto layout : layouts)
      renderLayout(&root, layout);
        
    for (auto set : buttonSets)
      renderButtonSet(&root, set);

    renderProperties(&root, properties);

    for (auto def : definitions)
      renderDefinition(&root, def);

    return root.toString();
}

void UIConfig::renderLayout(juce::XmlElement* parent, DisplayLayout* layout)
{
    juce::XmlElement* root = new juce::XmlElement("Layout");
    parent->addChildElement(root);

    if (layout->name.length() > 0) root->setAttribute("name", layout->name);

    for (auto element : layout->mainElements)
      renderElement(root, element);

    for (auto strip : layout->strips)
      renderStrip(root, strip);

    if (layout->instantParameters.size() > 0) {
        juce::XmlElement* ip = new juce::XmlElement("InstantParameters");
        ip->setAttribute("names", layout->instantParameters.joinIntoString(","));
        root->addChildElement(ip);
    }
}

void UIConfig::renderElement(juce::XmlElement* parent, DisplayElement* el)
{
    juce::XmlElement* root = new juce::XmlElement("Element");
    parent->addChildElement(root);

    // Reduce clutter by suppressing empty strings and zeros
    // need a utility for this
    if (el->type.length() > 0) root->setAttribute("type", el->type);
    if (el->name.length() > 0) root->setAttribute("name", el->name);
    if (el->x > 0) root->setAttribute("x", el->x);
    if (el->y > 0) root->setAttribute("y", el->y);
    if (el->width > 0) root->setAttribute("width", el->width);
    if (el->height > 0) root->setAttribute("height", el->height);
    if (el->disabled) root->setAttribute("disabled", el->disabled);
}

void UIConfig::renderStrip(juce::XmlElement* parent, DisplayStrip* strip)
{
    juce::XmlElement* root = new juce::XmlElement("Strip");
    parent->addChildElement(root);
    
    if (strip->name.length() > 0) root->setAttribute("name", strip->name);

    for (auto element : strip->elements)
      renderElement(root, element);
}

void UIConfig::renderButtonSet(juce::XmlElement* parent, ButtonSet* set)
{   
    juce::XmlElement* root = new juce::XmlElement("ButtonSet");
    parent->addChildElement(root);
    
    if (set->name.length() > 0) root->setAttribute("name", set->name);

    for (auto button : set->buttons)
      renderButton(root, button);
}

void UIConfig::renderButton(juce::XmlElement* parent, DisplayButton* button)
{
    juce::XmlElement* root = new juce::XmlElement("Button");
    parent->addChildElement(root);

    if (button->name.length() > 0) root->setAttribute("name", button->name);
    if (button->action.length() > 0) root->setAttribute("action", button->action);
    if (button->arguments.length() > 0) root->setAttribute("arguments", button->arguments);
    if (button->scope.length() > 0) root->setAttribute("scope", button->scope);
    // note that ARGB values with the high bit set will be negative
    if (button->color !=  0) root->setAttribute("color", juce::String(button->color));
}

void UIConfig::renderProperties(juce::XmlElement* parent, juce::HashMap<juce::String, juce::String>& props)
{
    if (props.size() > 0) {
        juce::XmlElement* root = new juce::XmlElement("Properties");
        parent->addChildElement(root);

        // figure out what begin() does...
        // this is from the docs
        // original implementation
#if 0        
        for (juce::HashMap<juce::String,juce::String>::Iterator i (props) ; i.next() ;) {
            
            juce::XmlElement* propel = new juce::XmlElement("Property");
            root->addChildElement(propel);
            propel->setAttribute("name", i.getKey());
            propel->setAttribute("value", i.getValue());
         }
#endif

        // XML serialization in Iterator order is unstable across machines
        // which leads to xml file differences when they are under source control
        // while it isn't necessary for this to be stable in normal use, I hit
        // it all the time developing over several machines and it can make Git merges harder
        // will want to encapsuate this somewhere if we have more than one 

        // first find and sort the keys
        juce::StringArray keys;
        for (juce::HashMap<juce::String,juce::String>::Iterator i (props) ; i.next() ;) {
            keys.add(i.getKey());
        }
        keys.sort(false);
        // now emit the map in this order, also filter out "null" empty strings
        for (auto key : keys) {
            juce::String value = props[key];
            if (value.length() > 0) {
                juce::XmlElement* propel = new juce::XmlElement("Property");
                root->addChildElement(propel);
                propel->setAttribute("name", key);
                propel->setAttribute("value", value);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////
//
// Element Definition Hackery
//
// See $OG/mobius/UITypes.cpp for the static definitions of these
// See StripElement.cpp in new code for some of the newer ones
//
// Until we can load these from XML, just hard code them  and make
// the names match what the code currently expects.
//
//////////////////////////////////////////////////////////////////////

/**
 * Until we have an XML representation for these, hard code the definitions.
 *
 * Old code had a bunch of static objects to define the set of names.
 * New code has been using classes that set the juce::Component id to
 * a constant string "ModeElement" etc.   These didn't come from defined constants,
 * just a const char* literal in the constructor.  The names we use here must match
 * those until the implementation classes can pull the names from some model.
 */
void UIConfig::hackDefinitions()
{
    definitions.add(new UIElementDefinition("ModeElement"));
    definitions.add(new UIElementDefinition("BeatersElement"));
    definitions.add(new UIElementDefinition("LoopMeterElement"));
    definitions.add(new UIElementDefinition("CounterElement"));
    definitions.add(new UIElementDefinition("FloatingStripElement"));
    definitions.add(new UIElementDefinition("ParametersElement"));
    definitions.add(new UIElementDefinition("AudioMeterElement"));
    definitions.add(new UIElementDefinition("LayerElement"));
    definitions.add(new UIElementDefinition("AlertElement"));
    definitions.add(new UIElementDefinition("MinorModesElement"));
    definitions.add(new UIElementDefinition("TempoElement"));
    definitions.add(new UIElementDefinition("LoopWindowElement"));


    // these were from old code and not yet implemented

    // don't remember what this was for
    // new DisplayElement("PresetAlert", MSG_UI_EL_PRESET_ALERT);

    // there was support for a second floating strip for pitch/speed knobs
    // do it a better way
    // new DisplayElement("TrackStrip2", MSG_UI_EL_TRACK_STRIP_2);
    
    // this was an extremly simple set of vertical bars representing
    // loops in the active track, not necessary with the loop status element
    // in the track strip
    // new DisplayElement("LoopBars", "LoopList", MSG_UI_EL_LOOP_BARS);
    
    // Definitions for items in the track strips were mostly just
    // parameter names except for these:
    //   FocusLockElement,
    //   TrackNumberElement,
	//   GroupNameElement,
    //   SmallLoopMeterElement,
    //   LoopRadarElement,
    //   OutputMeterElement,
	//   LoopStatusElement,

    // new code did have a set of static definitions for the ones we
    // supported, see StripElemenetDefinition in StripElement.cpp
    // those can all go away once the XML-based definition is finished,
    // all code needs is a set of string constnats for the name
    //
    // There were a lot of things defined, but we only implemented these
    // so only need to include things here that have implementations
    
    definitions.add(new UIElementDefinition("trackNumber", true));
    definitions.add(new UIElementDefinition("focusLock", true));
    definitions.add(new UIElementDefinition("loopRadar", true));
    definitions.add(new UIElementDefinition("loopMeter", true));
    definitions.add(new UIElementDefinition("loopStack", true));
    definitions.add(new UIElementDefinition("output", true));
    definitions.add(new UIElementDefinition("input", true));
    definitions.add(new UIElementDefinition("feedback", true));
    definitions.add(new UIElementDefinition("altFeedback", true));
    definitions.add(new UIElementDefinition("pan", true));
    definitions.add(new UIElementDefinition("outputMeter", true));
    definitions.add(new UIElementDefinition("inputMeter", true));
    definitions.add(new UIElementDefinition("groupName", true));

    // todo: derive the availableParameters list from Symbols marked
    // in some way or maybe just keep a static list
    
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
