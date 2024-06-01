/**
 * All binding panels share a common structure
 * They are ConfigPanels so have Save/Cancel buttons in the footer.
 * They have an optional object selector for bindings that have more than one object.
 *
 * On the left is a large scrolling binding table with columns for
 *
 *    Target
 *    Trigger
 *    Scope
 *    Arguments
 * 
 * Under the Target table are buttons New, Update, Delete to manage rows in the table.
 *
 * Under the BindingTargetPanel are Extended Fields to add additional information about
 * the Binding.  At minimum it will have an Arguments field to specifify arbitrary
 * trigger.
 *
 * todo: with the introduction of Symbols, an existing binding may be "unresolved"
 * if it has a name that does not correspond to a resolved Symbol.  Need to
 * display those in red or something.
 *
 */

#include <JuceHeader.h>

#include <string>
#include <sstream>

#include "../../util/Trace.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Binding.h"
#include "../common/Form.h"
#include "../JuceUtil.h"

#include "ConfigEditor.h"
#include "BindingTable.h"
#include "BindingTargetPanel.h"

#include "BindingPanel.h"

BindingPanel::BindingPanel(ConfigEditor* argEditor, const char* title, bool multi) :
    ConfigPanel{argEditor, title, ConfigPanelButton::Save | ConfigPanelButton::Cancel, multi}
{
    setName("BindingPanel");

    bindings.setListener(this);
    content.addAndMakeVisible(bindings);
    
    content.addAndMakeVisible(targets);

    content.addAndMakeVisible(form);
    // because of the use of subclass virtuals to build parts
    // of the form, we have to complete construction before calling initForm
    // subclass must do this instead
    // initForm();

    // default help area is a bit tall for the older layouts
    setHelpHeight(12);
    
    // we can either auto size at this point or try to
    // make all config panels a uniform size
    setSize (900, 600);
}

BindingPanel::~BindingPanel()
{
}

/**
 * ConfigPanel overload to prepare the panel to be shown.
 */
void BindingPanel::load()
{
    if (!loaded) {
        MobiusConfig* config = editor->getMobiusConfig();

        maxTracks = config->getTracks();
        maxGroups = config->getTrackGroups();

        targets.load();

        // until we support overlay editing, just start with
        // the first base binding set
        BindingSet* baseBindings = config->getBindingSets();
        if (baseBindings != nullptr) {
            Binding* blist = baseBindings->getBindings();
            while (blist != nullptr) {
                // subclass overload
                if (isRelevant(blist)) {
                    // table will copy
                    bindings.add(blist);
                }
                blist = blist->getNext();
            }
            bindings.updateContent();
        }

        // make sure the form is reset from the last time
        resetForm();

        // force this true for testing
        changed = true;
        loaded = true;
    }
}

void BindingPanel::save()
{
    if (changed) {
        MobiusConfig* config = editor->getMobiusConfig();

        // only dealing with the base binding set atm
        BindingSet* baseBindings = config->getBindingSets();
        if (baseBindings == nullptr) {
            // boostrap one
            baseBindings = new BindingSet();
            config->addBindingSet(baseBindings);
        }

        // note well: unlike most strings, setBingings() does
        // NOT delete the current Binding list, it just takes the pointer
        // so we can reconstruct the list and set it back without worrying
        // about dual ownership.  deleting a Binding DOES however follow the
        // chain so be careful with that.  Really need model cleanup
        juce::Array<Binding*> newBindings;

        Binding* original = baseBindings->getBindings();
        baseBindings->setBindings(nullptr);
        
        while (original != nullptr) {
            // take it out of the list to prevent cascaded delete
            Binding* next = original->getNext();
            original->setNext(nullptr);
            if (!isRelevant(original))
              newBindings.add(original);
            else
              delete original;
            original = next;
        }
        //traceBindingList("filtered", newBindings);
        
        // now add back the edited ones, some may have been deleted or added
        Binding* edited = bindings.captureBindings();
        //traceBindingList("from table", edited);
        while (edited != nullptr) {
            newBindings.add(edited);
            edited = edited->getNext();
        }

        //traceBindingList("merged", newBindings);

        // link them back up
        Binding* merged = nullptr;
        Binding* last = nullptr;
        for (int i = 0 ; i < newBindings.size() ; i++) {
            Binding* b = newBindings[i];
            // clear any residual chain
            b->setNext(nullptr);
            if (last == nullptr)
              merged = b;
            else
              last->setNext(b);
            last = b;
        }

        //traceBindingList("final", merged);

        // store the merged list back
        baseBindings->setBindings(merged);
        
        editor->saveMobiusConfig();

        changed = false;
        loaded = false;
    }
    else if (loaded) {
        // throw away the copies
        cancel();
    }
}

// temporary debugging hacks when flailing away at some problem
// not necessary

void BindingPanel::traceBindingList(const char* title, Binding* blist)
{
    int count = 0;
    trace("*** Bindings %s\n", title);
    while (blist != nullptr) {
        trace("%s\n", blist->getSymbolName());
        blist = blist->getNext();
        count++;
    }
    trace("*** %s %d total bindings\n", title, count);
}

void BindingPanel::traceBindingList(const char* title, juce::Array<Binding*> &blist)
{
    trace("*** Bindings %s\n", title);
    for (int i = 0 ; i < blist.size() ; i++) {
        Binding* b = blist[i];
        trace("%s\n", b->getSymbolName());
    }
    trace("*** %s %d total bindings\n", title, blist.size());
}

void BindingPanel::cancel()
{
    // throw away the copies
    Binding* blist = bindings.captureBindings();
    delete blist;
    changed = false;
    loaded = false;
}

//////////////////////////////////////////////////////////////////////
//
// Trigger/Scope/Arguments Form
//
//////////////////////////////////////////////////////////////////////

/**
 * Build out the form containing scope, subclass specific fields, 
 * and binding arguments.
 *
 * Don't have a Form interface that allows static Field objects
 * so we have to allocate them and let the Form own them.
 */
void BindingPanel::initForm()
{
    // scope always goes first
    juce::StringArray scopeNames;
    scopeNames.add("Global");

    if (maxTracks == 0) maxTracks = 8;
    for (int i = 0 ; i < maxTracks ; i++)
      scopeNames.add("Track " + juce::String(i+1));

    if (maxGroups == 0) maxGroups = 2;
    for (int i = 0 ; i < maxGroups ; i++) {
        // passing a char to the String constructor didn't work,
        // it rendered the numeric value of the character
        // operator += works
        // juce::String((char)('A' + i)));
        juce::String letter;
        letter += (char)('A' + i);
        scopeNames.add("Group " + letter);
    }
    
    scope = new Field("Scope", Field::Type::String);
    scope->setAllowedValues(scopeNames);
    form.add(scope);

    // subclass gets to add it's fields
    addSubclassFields();

    // arguments last
    arguments = new Field("Arguments", Field::Type::String);
    arguments->setWidthUnits(20);
    form.add(arguments);
    form.render();
}

/**
 * Reset all trigger and target arguments to their initial state
 */
void BindingPanel::resetForm()
{
    scope->setValue(juce::var(0));
    
    targets.reset();

    resetSubclassFields();
    
    arguments->setValue(juce::var());
}

/**
 * Refresh form to have values for the selected binding
 *
 * Binding model represents scopes as a string, then parses
 * that into track or group numbers.  
 */
void BindingPanel::refreshForm(Binding* b)
{
    const char* scopeName = b->getScope();
    if (scopeName == nullptr) {
        scope->setValue(juce::var(0));
    }
    else {
        // can assume these have been set as a side effect
        // of calling setScope(char), don't like this
        // track and group numbers are 1 based if they are set
        int tracknum = b->trackNumber;
        if (tracknum > 0) {
            // element 0 is "global" so track number works
            scope->setValue(juce::var(tracknum));
        }
        else {
            int groupnum = b->groupOrdinal;
            if (groupnum > 0)
              scope->setValue(juce::var(maxTracks + groupnum));
        }
    }
    
    targets.select(b);
    refreshSubclassFields(b);
    
    juce::var args = juce::var(b->getArguments());
    arguments->setValue(args);
    // used this in old code, now that we're within a form still necessary?
    // arguments.repaint();
}

/**
 * Copy what we have displayed for targets, scopes, and arguments
 * into a Binding.
 *
 * Binding currently wants scopes represented as a String
 * with tracks as numbers "1", "2", etc. and groups as
 * letters "A", "B", etc.
 *
 * Would prefer to avoid this, have bindings just store
 * the two numbers would make it easier to deal with but
 * not as readable in the XML.
 */
void BindingPanel::captureForm(Binding* b)
{
    // item 0 global, tracks, groups
    int item = scope->getIntValue();
    if (item == 0) {
        // global, this ensures both track and group are cleared
        b->setScope(nullptr);
    }
    else if (item <= maxTracks) {
        // this will format the track letter
        b->setTrack(item);
    }
    else {
        // the group number in the Binding starts with 1
        b->setGroup(item - maxTracks);
    }

    targets.capture(b);

    // todo: scope
    captureSubclassFields(b);
    
    juce::var value = arguments->getValue();
    b->setArguments(value.toString().toUTF8());
}

//////////////////////////////////////////////////////////////////////
//
// BindingTable Listener
//
//////////////////////////////////////////////////////////////////////

/**
 * Render the cell that represents the binding trigger.
 */
juce::String BindingPanel::renderTriggerCell(Binding* b)
{
    // subclass must overload this
    return renderSubclassTrigger(b);
}

/**
 * Update the binding info components to show things for the
 * binding selected in the table
 */
void BindingPanel::bindingSelected(Binding* b)
{
    if (bindings.isNew(b)) {
        // uninitialized row, don't modify it but reset the target display
        resetForm();
    }
    else {
        refreshForm(b);
    }
    
}

Binding* BindingPanel::bindingNew()
{
    Binding* neu = nullptr;
    
    // here we have the option of doing an immediate capture
    // of what is in the target selectors, return null
    // to leave a [New] placeholder row
    bool captureCurrentTarget = false;

    if (captureCurrentTarget && targets.isTargetSelected()) {
        neu = new Binding();
        captureForm(neu);
    }
    else {
        // we'l let BindingTable make a placeholder row
        // clear any lingering target selection?
        resetForm();
    }

    return neu;
}

void BindingPanel::bindingUpdate(Binding* b)
{
    // was ignoring this if !target.isTargetSelected
    // but I suppose we can go ahead and capture what we have
    captureForm(b);
}

void BindingPanel::bindingDelete(Binding* b)
{
    (void)b;
    resetForm();
}

/**
 * Field::Listener callback
 * If there is something selected in the table could actively
 * change it, but we're going with a manual Update button for now
 * now that we have a Form this would have to be a lot more
 * complicated
 */
void BindingPanel::fieldChanged(Field* field)
{
    (void)field;
}

//////////////////////////////////////////////////////////////////////
//
// Component
//
//////////////////////////////////////////////////////////////////////

void BindingPanel::resized()
{
    ConfigPanel::resized();
    
    juce::Rectangle<int> area = getLocalBounds();

    // leave some space at the top
    area.removeFromTop(20);
    // and on the left
    area.removeFromLeft(10);

    // let's fix the size of the table for now rather
    // adapt to our size
    int width = bindings.getPreferredWidth();
    int height = bindings.getPreferredHeight();

    // give the targets more room
    width -= 50;
    bindings.setBounds(area.getX(), area.getY(), width, height);

    area.removeFromLeft(bindings.getWidth() + 10);
    // need enough room for arguments so shorten it
    // could try to adapt to the size of the argumnts Form instead
    targets.setBounds(area.getX(), area.getY(), 400, 300);

    //form->render();
    form.setTopLeftPosition(area.getX(), targets.getY() + targets.getHeight() + 10);
}    

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
