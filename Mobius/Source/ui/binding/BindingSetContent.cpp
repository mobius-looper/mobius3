
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Binding.h"
#include "../../model/BindingSet.h"
#include "../../model/Symbol.h"
#include "../../Provider.h"

#include "BindingTable.h"
#include "BindingTree.h"
#include "BindingEditor.h"

#include "BindingSetContent.h"

BindingSetContent::BindingSetContent()
{
}

void BindingSetContent::initialize(bool argButtons)
{
    buttons = argButtons;
    
    if (buttons) {
        addAndMakeVisible(buttonTable);
    }
    else {
        addAndMakeVisible(tabs);
        tabs.add("MIDI", &midiTable);
        tabs.add("Keyboard", &keyTable);
        tabs.add("Plugin Parameter", &hostTable);
    }
}

void BindingSetContent::load(class BindingEditor* ed, class BindingSet* set)
{
    editor = ed;
    bindingSet = set;
    if (buttons) {
        buttonTable.load(ed, set, BindingTable::TypeButton);
    }
    else {
        midiTable.load(ed, set, BindingTable::TypeMidi);
        keyTable.load(ed, set, BindingTable::TypeKey);
        hostTable.load(ed, set, BindingTable::TypeHost);
    }
}

void BindingSetContent::cancel()
{
    midiTable.cancel();
    keyTable.cancel();
    hostTable.cancel();
    buttonTable.cancel();
}

/**
 * Here indirectly from BindingEditor after BindingEditor has finished editing
 * an object and wants it saved.
 *
 * This was a COPY of the Binding from the set managed here.
 * We do not get to own it.
 */
void BindingSetContent::save(Binding& edited)
{
    BindingTable* table = nullptr;
    if (edited.trigger == Binding::TriggerKey) {
        table = &midiTable;
    }
    else if (edited.trigger == Binding::TriggerHost) {
        table = &hostTable;
    }
    else if (edited.trigger == Binding::TriggerNote ||
             edited.trigger == Binding::TriggerControl ||
             edited.trigger == Binding::TriggerProgram) {
        table = &midiTable;
    }
    else if (edited.trigger == Binding::TriggerUI) {
        table = &buttonTable;
    }
    else {
        Trace(1, "BindingSetContent: Attempt to save binding with an invalid trigger");
    }

    if (table != nullptr) {
        if (edited.uid == 0) {
            // a new one, table will add it in the right location
            table->finishCreate(edited);
        }
        else {
            Binding* src = bindingSet->findByUid(edited.uid);
            if (src == nullptr) {
                // not normal, if the BindingDeatails popup managed to make it all the
                // way back here, then a uid should have been assigned on the way out
                Trace(1, "BindingSetContent: Attempt to save binding with no matching uid");
            }
            else {
                src->copy(&edited);
                table->refresh();
            }
        }
    }
}

void BindingSetContent::resized()
{
    if (buttons)
      buttonTable.setBounds(getLocalBounds());
    else
      tabs.setBounds(getLocalBounds());
}

#if 0
bool BindingSetContent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    bool interested = false;
    juce::String src = details.description.toString();
    if (src.startsWith(BindingTree::DragPrefix))
      interested = true;
    return interested;
}
#endif

/**
 * So we don't have enough awareness to fully process the drop, so forward back
 * to a Listener.
 *
 * NOTE WELL:
 *
 * There are two drop targets in the binding windows.  This one and
 * BindingTable itself.  BindingTable gets control only when the table was built
 * for the ButtonsEditor which has an ordered table and dropping is much more complicaated.
 *
 * Here we simply insert it sorted.  This is messy.
 */
#if 0
void BindingSetContent::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    //if (listener != nullptr)
    //listener->parameterFormDrop(this, details.description.toString());

    //Trace(2, "BindingSetContent: Item dropped %s", details.description.toString().toUTF8());

    juce::String src = details.description.toString();

    if (src.startsWith(BindingTree::DragPrefix)) {
            
        juce::String sname = src.fromFirstOccurrenceOf(BindingTree::DragPrefix, false, false);
        Binding::Trigger trigger = Binding::TriggerUnknown;
        
        if (midiTable.isVisible()) {
            trigger = Binding::TriggerNote;
        }
        else if (keyTable.isVisible()) {
            trigger = Binding::TriggerKey;
        }
        else if (hostTable.isVisible()) {
            trigger = Binding::TriggerHost;
        }
        else if (buttonTable.isVisible()) {
            trigger = Binding::TriggerUI;
        }

        if (trigger != Binding::TriggerUnknown) {
            Binding b;
            b.symbol = sname;
            b.trigger = trigger;
            editor->createBinding(b);
        }
        else {
            Trace(1, "BindingSetContent: Problem finding binding table");
        }
    }
    else {
        // drag from something other than the BindingTree
        // this isn't expected, the BindingTable can drag onto itself but that
        // is only enabled when within ButtonsEditor
        Trace(1, "BindingSetContent::itemDropped Drop from unknown source %s",
              src.toUTF8());
    }
}
#endif

