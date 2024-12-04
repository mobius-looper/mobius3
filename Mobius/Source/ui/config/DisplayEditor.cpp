/**
 * The DisplayPanel provides editing components for the following things
 *
 * Display Elements
 *    the random moveable things that you can have in the main display:
 *      loop meter, counter, beaters, floating track strip, etc.
 *
 * Static Track Strip
 *    the random unmovable things you can have stacked in the track strips
 *    along the bottom of the display
 *
 * Floating Track Strip
 *    the things you can have stacked in a track strip that can be moved around
 *    withou the display area, this always shows state for the active track
 *
 * Instant Parameters
 *    runtime parameters for the active track that are displayed and editable in
 *    the the Instant Parameters element, when it is visible
 *
 * Old code had a "Floating Track Strip 2" which was a second floater that could
 * contain different things.  Leaving that out for now, if we do this at all
 * should allow any number.  This could allow you to have many track "strips" containing
 * a single element that can be organized and sized as desired.
 * But really, why not let everything in the track strip just be first-class
 * display elements like the others?  Would be cool to have a gigantic loop radar
 * if that's what you want.
 *
 * Future goals:
 *
 * Elements in the main area should be resizeable and have customizeable colors and
 * maybe fonts.  Some of these are rather complicated so would need a complex set
 * of editing panels for each.
 *
 * Items in the docked track strip at the bottom must have uniform size and will not
 * always follow size preferences used when they are floating.  But could allow limited
 * size preferences.
 *
 * Width and height of the track strips is auto calculated based on what it contains,
 * could allow preferences to force them wider or taller than the minimum required.
 *
 * !! I really like the notion that track strips are just bundles of display elements
 * and there is nothing that can be in them that can't be a standalone element in the display.
 *
 * It's a problem now, but once we allow size preferences it will be easy for elements
 * to overlap and require the user to rearrange them.  Need some form of "collision detection"
 * where elements can't be moved over the top of another, or perhaps cooler, they "push"
 * the others out of the way when you move them.
 *
 * And of course once size becomes configurable why just just let them be drag sized like
 * windows.
 */
 
#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../util/Trace.h"
#include "../../util/List.h"
#include "../../model/UIConfig.h"
#include "../../model/Symbol.h"
#include "../../Supervisor.h"

#include "DisplayEditor.h"

DisplayEditor::DisplayEditor(Supervisor* s) : ConfigEditor(s)
{
    setName("DisplayEditor");

    properties.setLabelColor(juce::Colours::orange);
    properties.setLabelCharWidth(10);
    properties.setTopInset(12);
    properties.add(&loopRows);
    properties.add(&trackRows);
    properties.add(&buttonHeight);
    properties.add(&alertDuration);

    tabs.add("Main Elements", &mainElements);
    tabs.add("Docked Track Strip", &dockedStrip);
    tabs.add("Floating Track Strip", &floatingStrip);
    tabs.add("Instant Parameters", &instantParameters);
    tabs.add("Properties", &properties);
                 
    addAndMakeVisible(&tabs);
}

DisplayEditor::~DisplayEditor()
{
}

void DisplayEditor::prepare()
{
    context->enableObjectSelector();
    context->enableHelp(40);

    HelpArea* help = context->getHelpArea();
    mainElements.setHelpArea(help, "displayEditorElements");
    dockedStrip.setHelpArea(help, "displayEditorDock");
    floatingStrip.setHelpArea(help, "displayEditorFloating");
    instantParameters.setHelpArea(help, "displayEditorParameters");
}

void DisplayEditor::load()
{
    UIConfig* config = supervisor->getUIConfig();

    // make a local copy of the DisplayLayouts for editing
    layouts.clear();
    revertLayouts.clear();
    for (auto layout : config->layouts) {
        layouts.add(new DisplayLayout(layout));
        revertLayouts.add(new DisplayLayout(layout));
    }

    // todo: really need to find a way to deal with "named object lists"
    // in a generic way with OwnedArray, c++ makes this too fucking hard
    // maybe some sort of transient container Map that also gets rid
    // of linear name searches
    selectedLayout = 0;
    juce::String active = config->activeLayout;
    if (active.length() > 0) {
        int index = 0;
        for (auto layout : config->layouts) {
            if (layout->name == active) {
                selectedLayout = index;
                break;
            }
            index++;
        }
    }

    loadLayout(selectedLayout);
    refreshObjectSelector();
}

/**
 * Refresh the object selector on initial load and after any
 * objects are added or removed.  This could be pushed up to
 * ConfigPanel if each subclass had a method to return
 * the list of names and the current selection, but at that point
 * you're not eliminating much duplication.
 */
void DisplayEditor::refreshObjectSelector()
{
    juce::Array<juce::String> names;
    for (auto layout : layouts) {
        if (layout->name.length() == 0)
          layout->name = "[New]";
        names.add(layout->name);
    }
    context->setObjectNames(names);
    context->setSelectedObject(selectedLayout);
}

void DisplayEditor::save()
{
    UIConfig* config = supervisor->getUIConfig();
        
    saveLayout(selectedLayout);

    config->activeLayout = (layouts[selectedLayout])->name;
    config->layouts.clear();
        
    while (layouts.size() > 0) {
        DisplayLayout* neu = layouts.removeAndReturn(0);
        config->layouts.add(neu);
    }

    supervisor->updateUIConfig();
}

void DisplayEditor::cancel()
{
    layouts.clear();
    revertLayouts.clear();
}

void DisplayEditor::revert()
{
    DisplayLayout* reverted = new DisplayLayout(revertLayouts[selectedLayout]);
    // !! wait, aren't we supposed to copy this like we do for the old model ?
    layouts.set(selectedLayout, reverted);
    loadLayout(selectedLayout);
    // in case the name was edited
    refreshObjectSelector();
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector Overloads
//
//////////////////////////////////////////////////////////////////////

void DisplayEditor::objectSelectorSelect(int ordinal)
{
    if (ordinal != selectedLayout) {
        saveLayout(selectedLayout);
        selectedLayout = ordinal;
        loadLayout(selectedLayout);
    }
}

void DisplayEditor::objectSelectorNew(juce::String newName)
{
    int newOrdinal = layouts.size();

    // this one is complex and likely to contain minor adjustments
    // so creating a new one starts with a copy of the old one
    DisplayLayout* neu = new DisplayLayout(layouts[selectedLayout]);
    neu->name = "[New]";

    layouts.add(neu);
    revertLayouts.add(new DisplayLayout(neu));
    
    selectedLayout = newOrdinal;
    loadLayout(selectedLayout);

    refreshObjectSelector();
}

void DisplayEditor::objectSelectorDelete()
{
    if (layouts.size() == 1) {
        // must have at least one
    }
    else {
        layouts.remove(selectedLayout);
        revertLayouts.remove(selectedLayout);
        // leave the index where it was and show the next one,
        // if we were at the end, move back
        int newOrdinal = selectedLayout;
        if (newOrdinal >= layouts.size())
          newOrdinal = layouts.size() - 1;
        selectedLayout = newOrdinal;
        loadLayout(selectedLayout);
        refreshObjectSelector();
    }
}

void DisplayEditor::objectSelectorRename(juce::String newName)
{
    DisplayLayout* layout = layouts[selectedLayout];
    layout->name = newName;
}

void DisplayEditor::resized()
{
    tabs.setBounds(getLocalBounds());
}

//////////////////////////////////////////////////////////////////////
//
// Internal Load
//
//////////////////////////////////////////////////////////////////////

/**
 * Load the layout into the editor components.
 * Have to get UIConfig every time to rebuild the allowed values list,
 * unfortunate.
 *
 * Dislike having to repopulate the allowed list every time, but the filtering
 * based on current becomes the model under the ListBox.  Would be better
 * if MultiSelectDrag could save the full allowed list and calculate the visible
 * allowed list when the current value is set.
 */
void DisplayEditor::loadLayout(int ordinal)
{
    DisplayLayout* layout = layouts[ordinal];
    
    mainElements.clear();
    dockedStrip.clear();
    floatingStrip.clear();
    instantParameters.clear();

    UIConfig* config = supervisor->getUIConfig();
    trackRows.setText(config->get("trackRows"));
    loopRows.setText(config->get("loopRows"));
    buttonHeight.setText(config->get("buttonHeight"));
    alertDuration.setText(config->get("alertDuration"));
    
    initElementSelector(&mainElements, config, layout->mainElements, false);

    DisplayStrip* docked = layout->getDockedStrip();
    initElementSelector(&dockedStrip, config, docked->elements, true);

    DisplayStrip* floating = layout->getFloatingStrip();
    initElementSelector(&floatingStrip, config, floating->elements, true);
    
    initParameterSelector(&instantParameters, config, layout->instantParameters);
}

/**
 * Populate one of the multi-selects with one of the lists
 * from the config object.
 *
 * Dislike having to repopulate the allowed list every time, but the filtering
 * based on current becomes the model under the ListBox.  Would be better
 * if MultiSelectDrag could save the full allowed list and calculate the visible
 * allowed list when the current value is set.
 */
void DisplayEditor::initElementSelector(MultiSelectDrag* multi, UIConfig* config,
                                        juce::OwnedArray<DisplayElement>& elements,
                                        bool trackStrip)
{
    // we'll do this three times, could cache it somewhere
    juce::StringArray allowed;
    for (auto def : config->definitions) {
        if (trackStrip == def->trackStrip)
          allowed.add(def->name);
    }

    juce::StringArray current;
    for (auto el : elements) {
        if (!el->disabled)
          current.add(el->name);
    }

    multi->setValue(current, allowed);
}

/**
 * Build the list of parameters allowed for inclusion in the
 * Instant Parameters element.
 *
 * By default we'll put every defined UIParameter in here, which will
 * be long and unweildy.  Allow this to be restricted to just the ones
 * commonly used.
 *
 * The lists will have display names if one is available.
 */
void DisplayEditor::initParameterSelector(MultiSelectDrag* multi, UIConfig* config,
                                          juce::StringArray values)
{
    juce::StringArray allowed;
    if (config->availableParameters.size() > 0) {
        for (auto name : config->availableParameters) {
            addParameterDisplayName(name, allowed);
        }
    }
    else {
        // fall back to all of them, less easy to navigate but it's a start
        for (auto symbol : supervisor->getSymbols()->getSymbols()) {
            if (symbol->behavior == BehaviorParameter)
              allowed.add(symbol->getDisplayName());
        }
    }

    // add exported script variables
    // this is one way to do it, other panels operate from the symbol table
    // which is probably better
    MslEnvironment* env = supervisor->getMslEnvironment();
    juce::Array<MslLinkage*> links = env->getLinks();
    for (auto link : links) {
        if (!link->isFunction) {
            allowed.add(link->name);
        }
    }

    // do a similar display name conversion on the current values
    juce::StringArray current;
    for (auto name : values) {
        addParameterDisplayName(name, current);
    }

    multi->setValue(current, allowed);
}

void DisplayEditor::addParameterDisplayName(juce::String name, juce::StringArray& values)
{
    Symbol* s = supervisor->getSymbols()->find(name);
    if (s == nullptr) {
        Trace(1, "DisplayEditor: Unresolved parameter %s\n", name.toUTF8());
    }
    else if (s->script != nullptr) {
        // these don't have display names
        values.add(s->name);
    }
    else if (s->behavior != BehaviorParameter) {
        Trace(1, "DisplayEditor: Symbol %s is not a parameter\n", name.toUTF8());
    }
    else {
        values.add(s->getDisplayName());
    }
}

//////////////////////////////////////////////////////////////////////
//
// Internal Save
//
//////////////////////////////////////////////////////////////////////

/**
 * Save editing state to the original model.
 * Names convey as-is for the display elements but for parameters we used the
 * display names so need to reverse map.
 */
void DisplayEditor::saveLayout(int ordinal)
{
    DisplayLayout* layout = layouts[ordinal];
    
    // annoying warning about "nonstandard extension used"
    // when passing the OwnedArray to a method that takes a reference
    // not sure what to do, I guess make it take a pointer?
    saveElements(layout->mainElements, mainElements.getValue());

    DisplayStrip* strip = layout->getDockedStrip();
    saveStripElements(strip->elements, dockedStrip.getValue());

    strip = layout->getFloatingStrip();
    saveStripElements(strip->elements, floatingStrip.getValue());

    // parameters need reverse display name mapping
    juce::StringArray displayNames = instantParameters.getValue();
    juce::StringArray symbolNames;
    for (auto dname : displayNames) {
        Symbol* s = findSymbolWithDisplayName(dname);
        if (s != nullptr)
          symbolNames.add(s->name);
    }
    layout->instantParameters = symbolNames;

    // this one is global
    UIConfig* config = supervisor->getUIConfig();

    // todo: really need to be constraining this to an integer before saving
    // if we're just using a text field
    config->put("trackRows", trackRows.getText());
    config->put("loopRows", loopRows.getText());
    config->put("buttonHeight", buttonHeight.getText());
    config->put("alertDuration", alertDuration.getText());
}

/**
 * Given a list of DisplayElements from a container and a list of
 * desired names, mark the ones still desired as enabled,
 * and the ones no longer desired as disabled.  If there is a new
 * name on the list create a new Element and enable it.
 */
void DisplayEditor::saveElements(juce::OwnedArray<DisplayElement>& elements,
                                 juce::StringArray names)
{
    // mark everything in the new list as enabled and create if necessary
    for (auto name : names) {
        // really wish this was a map
        DisplayElement* existing = nullptr;
        for (auto el : elements) {
            if (el->name == name) {
                existing = el;
                break;
            }
        }
        if (existing != nullptr) {
            existing->disabled = false;
        }
        else {
            DisplayElement* el = new DisplayElement();
            el->name = name;
            // don't have a good way to position these, just leave it at 0,0
            // doesn't matter for DisplayStrip elements
            elements.add(el);
        }
    }

    // anything not in the new list is marked as disabled
    for (auto el : elements) {
        if (!names.contains(el->name)) {
            el->disabled = true;
        }
    }
}

/**
 * Unlike main status elements, strip elements are ordered.
 * This is touchy because we're dealing with OwnedArray and reordering it
 * as well as adding new elements and iterating over it.
 * 
 * Start by copying the current elements to a temporary array, clearing the OwnedArray without
 * deleting the elements, then rebuilding it in order.
 */
void DisplayEditor::saveStripElements(juce::OwnedArray<DisplayElement>& elements,
                                      juce::StringArray names)
{
    juce::OwnedArray<DisplayElement> existing;

    // move the elements to the temporary array
    while (elements.size() > 0)
      existing.add(elements.removeAndReturn(0));

    // rebuild it with the new order
    for (auto name : names) {
        // really wish this was a map
        DisplayElement* found = nullptr;
        for (auto el : existing) {
            if (el->name == name) {
                found = el;
                break;
            }
        }
        
        if (found != nullptr) {
            existing.removeObject(found, false);
            elements.add(found);
            found->disabled = false;
        }
        else {
            DisplayElement* el = new DisplayElement();
            el->name = name;
            elements.add(el);
        }
    }

    // anything not in the new list is marked as disabled
    while (existing.size() > 0) {
        DisplayElement* el = existing.removeAndReturn(0);
        elements.add(el);
        el->disabled = true;
    }
}

/**
 * Look for a UIParameter attached to a Symbol using the display name.
 * Might want a Map for this someaday since the symbol list can
 * be long, but this doesn't happen often.
 */
Symbol* DisplayEditor::findSymbolWithDisplayName(juce::String dname)
{
    Symbol* found = nullptr;

    for (auto symbol : supervisor->getSymbols()->getSymbols()) {
        if (dname == symbol->getDisplayName()) {
            found = symbol;
            break;
        }
    }
    if (found == nullptr)
      Trace(1, "DisplayEditor: Unable to locate symbol with display name %s\n",
            dname.toUTF8());

    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

