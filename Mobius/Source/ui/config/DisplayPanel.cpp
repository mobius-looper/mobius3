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
#include "../../model/UIParameter.h"
#include "../../model/Symbol.h"
#include "../../Supervisor.h"

#include "ConfigEditor.h"

#include "DisplayPanel.h"

const bool DisplayPanelTabs = true;


DisplayPanel::DisplayPanel(ConfigEditor* argEditor) :
    ConfigPanel{argEditor, "Displays", ConfigPanelButton::Save | ConfigPanelButton::Cancel, true}
{
    setName("DisplayPanel");

    setMainContent(&displayEditor);
    
    // we can either auto size at this point or try to
    // make all config panels a uniform size
    setSize (900, 600);
}

DisplayPanel::~DisplayPanel()
{
}

/**
 * Simpler than Presets and Setups because we don't have multiple objects to deal with.
 * Load fields from the master config at the start, then commit them directly back
 * to the master config.
 */
void DisplayPanel::load()
{
    if (!loaded) {

        // until we finish the transition, fake up the new model temporarily
        UIConfig* config = editor->getUIConfig();

        // make a local copy of the DisplayLayout for editing
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

        refreshObjectSelector();
        loadLayout(selectedLayout);
        
        // force this true for testing
        changed = true;
        loaded = true;
    }
}

/**
 * Refresh the object selector on initial load and after any
 * objects are added or removed.  This could be pushed up to
 * ConfigPanel if each subclass had a method to return
 * the list of names and the current selection, but at that point
 * you're not elimiating much duplication.
 */
void DisplayPanel::refreshObjectSelector()
{
    juce::Array<juce::String> names;
    for (auto layout : layouts) {
        if (layout->name.length() == 0)
          layout->name = "[New]";
        names.add(layout->name);
    }
    // this will also auto-select the first one
    objectSelector.setObjectNames(names);
    objectSelector.setSelectedObject(selectedLayout);
}

void DisplayPanel::save()
{
    if (changed) {
        UIConfig* config = editor->getUIConfig();
        
        saveLayout(selectedLayout);

        config->activeLayout = (layouts[selectedLayout])->name;
        config->layouts.clear();
        
        while (layouts.size() > 0) {
            DisplayLayout* neu = layouts.removeAndReturn(0);
            config->layouts.add(neu);
        }

        editor->saveUIConfig();
        
        changed = false;
        loaded = false;
    }
}

void DisplayPanel::cancel()
{
    changed = false;
    // remember, this is the thing that makes ConfigEditor hide us
    // !! need to retool this to make the meaning of this flag clearer
    loaded = false;
}

void DisplayPanel::loadLayout(int ordinal)
{
    DisplayLayout* layout = layouts[ordinal];
    displayEditor.load(layout);
}

void DisplayPanel::saveLayout(int ordinal)
{
    DisplayLayout* layout = layouts[ordinal];
    displayEditor.save(layout);
}

//////////////////////////////////////////////////////////////////////
//
// ObjectSelector Overloads
//
//////////////////////////////////////////////////////////////////////

/**
 * Called when the combobox changes.
 */
void DisplayPanel::selectObject(int ordinal)
{
    if (ordinal != selectedLayout) {
        saveLayout(selectedLayout);
        selectedLayout = ordinal;
        loadLayout(selectedLayout);
    }
}

void DisplayPanel::newObject()
{
    int newOrdinal = layouts.size();

    // this one is complex and likely to contain minor adjustments
    // so creating a new one starts with a copy of the old one
    DisplayLayout* neu = new DisplayLayout(layouts[selectedLayout]);
    neu->name = "[New]";

    layouts.add(neu);
    // make another copy for revert
    revertLayouts.add(new DisplayLayout(neu));
    
    objectSelector.addObjectName(neu->name);
    // select the one we just added
    objectSelector.setSelectedObject(newOrdinal);
    selectedLayout = newOrdinal;
    loadLayout(selectedLayout);
    refreshObjectSelector();
}

/**
 * Delete is somewhat complicated.
 * You can't undo it unless we save it somewhere.
 * An alert would be nice, ConfigPanel could do that.
 */
void DisplayPanel:: deleteObject()
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

void DisplayPanel::revertObject()
{
    DisplayLayout* reverted = new DisplayLayout(revertLayouts[selectedLayout]);
    layouts.set(selectedLayout, reverted);
    // what about the ObjectSelector name!!
    loadLayout(selectedLayout);
    // in case the name was edited
    refreshObjectSelector();
}

/**
 * Called when the ObjectSelector's ComboBox changed the name.
 */
void DisplayPanel::renameObject(juce::String newName)
{
    DisplayLayout* layout = layouts[selectedLayout];
    layout->name = objectSelector.getObjectName();
}

//////////////////////////////////////////////////////////////////////
//
// DisplayEditor
//
//////////////////////////////////////////////////////////////////////

DisplayEditor::DisplayEditor()
{
    mainElements.setHelpArea(&helpArea, "displayEditorElements");
    dockedStrip.setHelpArea(&helpArea, "displayEditorDock");
    floatingStrip.setHelpArea(&helpArea, "displayEditorFloating");
    instantParameters.setHelpArea(&helpArea, "displayEditorParameters");
    
    if (DisplayPanelTabs) {
        addAndMakeVisible(&tabs);

        tabs.add("Main Elements", &mainElements);
        tabs.add("Docked Track Strip", &dockedStrip);
        tabs.add("Floating Track Strip", &floatingStrip);
        tabs.add("Instant Parameters", &instantParameters);

        // need an extensible name/value editor here or drive
        // it from a model
        tabs.add("Properties", &trackRows);
                 
        addAndMakeVisible(helpArea);
    }
    else {
        mainElements.setLabel(juce::String("Display Elements"));
        addAndMakeVisible(mainElements);
        
        dockedStrip.setLabel(juce::String("Docked Track Strip"));
        addAndMakeVisible(dockedStrip);
        
        floatingStrip.setLabel(juce::String("Floating Track Strip"));
        addAndMakeVisible(floatingStrip);
        
        instantParameters.setLabel(juce::String("Instant Parameters"));
        addAndMakeVisible(instantParameters);
        
        addAndMakeVisible(helpArea);
    }
}

DisplayEditor::~DisplayEditor()
{
}

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
void DisplayEditor::load(DisplayLayout* layout)
{
    // why not do this earlier?
    helpArea.setCatalog(Supervisor::Instance->getHelpCatalog());
    
    mainElements.clear();
    dockedStrip.clear();
    floatingStrip.clear();
    instantParameters.clear();

    UIConfig* config = Supervisor::Instance->getUIConfig();
    trackRows.setText(config->get("trackRows"));
    
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
        for (auto symbol : Symbols.getSymbols()) {
            UIParameter* p = symbol->parameter;
            if (p != nullptr) {
                const char* dname = p->getDisplayName();
                if (dname != nullptr) 
                  allowed.add(juce::String(dname));
                else
                  allowed.add(symbol->name);
            }
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
    Symbol* s = Symbols.find(name);
    if (s == nullptr) {
        Trace(1, "DisplayPanel: Unresolved parameter %s\n", name.toUTF8());
    }
    else if (s->parameter == nullptr) {
        Trace(1, "DisplayPanel: Symbol %s is not a parameter\n", name.toUTF8());
    }
    else {
        UIParameter* p = s->parameter;
        const char* dname = p->getDisplayName();
        if (dname != nullptr) 
          values.add(juce::String(dname));
        else
          values.add(s->name);
    }
}

void DisplayEditor::resized()
{
    if (DisplayPanelTabs) {
        juce::Rectangle<int> area = getLocalBounds();
        int helpHeight = 40;
        //area.reduced(20);

        juce::Rectangle<int> helpBounds = area.removeFromBottom(helpHeight);
        // inset it a little
        (void)helpBounds.reduced(2);
        helpArea.setBounds(helpBounds);

        // start here till we get more in the form
        // area.removeFromRight(area.getWidth() / 2);

        tabs.setBounds(area);
    }
    else {
        juce::Rectangle<int> area = getLocalBounds();
        int helpHeight = 40;
        //area.reduced(20);

        juce::Rectangle<int> helpBounds = area.removeFromBottom(helpHeight);
        // inset it a little
        (void)helpBounds.reduced(2);
        helpArea.setBounds(helpBounds);

        // start here till we get more in the form
        area.removeFromRight(area.getWidth() / 2);
        int unit = area.getHeight() / 4;
        int gap = 4;
        int multiHeight = unit - gap;
    
        mainElements.setBounds(area.removeFromTop(multiHeight));
        area.removeFromTop(gap);
        dockedStrip.setBounds(area.removeFromTop(multiHeight));
        area.removeFromTop(gap);
        floatingStrip.setBounds(area.removeFromTop(multiHeight));
        area.removeFromTop(gap);
        instantParameters.setBounds(area.removeFromTop(multiHeight));
    }
}

void DisplayEditor::paint(juce::Graphics& g)
{
    (void)g;
}

/**
 * Save editing state to the old model.
 * Names convey as-is for the display elements but for parameters we used the
 * display names so need to reverse map.
 */
void DisplayEditor::save(DisplayLayout* layout)
{
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
        UIParameter* param = findParameterWithDisplayName(dname);
        if (param != nullptr)
          symbolNames.add(param->getName());
    }
    layout->instantParameters = symbolNames;

    // this one is global
    UIConfig* config = Supervisor::Instance->getUIConfig();
    config->put("trackRows", trackRows.getText());
    
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
 */
void DisplayEditor::saveStripElements(juce::OwnedArray<DisplayElement>& elements,
                                      juce::StringArray names)
{
    juce::Array<DisplayElement*> newElements;
    
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
            newElements.add(existing);
            elements.removeObject(existing, false);
        }
        else {
            DisplayElement* el = new DisplayElement();
            el->name = name;
            newElements.add(el);
        }
    }

    // anything not in the new list is marked as disabled
    for (auto el : elements) {
        if (!names.contains(el->name)) {
            el->disabled = true;
            newElements.add(el);
            elements.removeObject(el, false);
        }
    }

    // should be empty
    if (elements.size() > 0)
      Trace(1, "DisplayEditor::saveStripElements Your code sucks and you have lingering elements\n");

    // put the new ordrered list back
    for (auto el : newElements)
      elements.add(el);
}

StringList* DisplayEditor::toStringList(juce::StringArray& src)
{
    StringList* list = nullptr;

    if (src.size() > 0) {
        list = new StringList();
        for (auto name : src)
          list->add(name.toUTF8());
    }
    return list;
}

/**
 * Look for a UIParameter attached to a Symbol using the display name.
 * Might want a Map for this someaday since the symbol list can
 * be long, but this doesn't happen often.
 */
UIParameter* DisplayEditor::findParameterWithDisplayName(juce::String dname)
{
    UIParameter* found = nullptr;

    for (auto symbol : Symbols.getSymbols()) {
        UIParameter* p = symbol->parameter;
        if (p != nullptr && (dname == juce::String(p->getDisplayName()))) {
            found = p;
            break;
        }
    }
    if (found == nullptr)
      Trace(1, "DisplayEditor: Unable to locate parameter with display name %s\n",
            dname.toUTF8());

    return found;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

