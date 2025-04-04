
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
        tabs.add("Host Parameter", &hostTable);
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
 * We get this notificiation in a roundabout way when a BindingDetails popup
 * that edited a single binding was saved.
 * Refresh the table it came from.
 */
void BindingSetContent::bindingSaved()
{
    if (midiTable.isVisible())
      midiTable.refresh();
    else if (keyTable.isVisible())
      keyTable.refresh();
    else if (hostTable.isVisible())
      hostTable.refresh();
    else if (buttonTable.isVisible())
      buttonTable.refresh();
}

void BindingSetContent::resized()
{
    if (buttons)
      buttonTable.setBounds(getLocalBounds());
    else
      tabs.setBounds(getLocalBounds());
}

bool BindingSetContent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    bool interested = false;
    juce::String src = details.description.toString();
    if (src.startsWith(BindingTree::DragPrefix))
      interested = true;
    return interested;
}

/**
 * So we don't have enough awareness to fully process the drop, so forward back
 * to a Listener.
 */
void BindingSetContent::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    //if (listener != nullptr)
    //listener->parameterFormDrop(this, details.description.toString());

    Trace(2, "BindingSetContent: Item dropped %s", details.description.toString().toUTF8());

    juce::String src = details.description.toString();
    juce::String name = src.fromFirstOccurrenceOf(BindingTree::DragPrefix, false, false);
    Symbol* s = editor->getProvider()->getSymbols()->find(name);
    if (s == nullptr) {
        Trace(1, "BindingSetContent: Invalid symbol name %s", name.toUTF8());
    }
    else {
        Binding* neu = new Binding();
        neu->symbol = name;

        BindingTable* table = nullptr;
        
        if (midiTable.isVisible()) {
            table = &midiTable;
            neu->trigger = Binding::TriggerNote;
        }
        else if (keyTable.isVisible()) {
            table = &keyTable;
            neu->trigger = Binding::TriggerKey;
        }
        else if (hostTable.isVisible()) {
            table = &hostTable;
            neu->trigger = Binding::TriggerHost;
        }
        else if (buttonTable.isVisible()) {
            table = &buttonTable;
            neu->trigger = Binding::TriggerUI;
        }

        if (table != nullptr) {
            bindingSet->add(neu);
            table->add(neu);
        }
        else {
            Trace(1, "BindingSetContent: Problem finding binding table");
            delete neu;
        }
    }
}

