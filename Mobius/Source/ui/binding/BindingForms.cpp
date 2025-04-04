
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Binding.h"
#include "../../model/GroupDefinition.h"
#include "../../model/Scope.h"
#include "../../model/Symbol.h"
#include "../../Provider.h"
#include "../MobiusView.h"

#include "BindingUtil.h"

#include "BindingForms.h"

BindingForms::BindingForms()
{
    title.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(title);

    triggerTitle.setText("Trigger", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(triggerTitle);
    
    targetTitle.setText("Target", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(targetTitle);

    juce::StringArray midiTypeNames;
    // could have an array of Triggers for these
    midiTypeNames.add("Note");
    midiTypeNames.add("Control");
    midiTypeNames.add("Program");
    midiType.setItems(midiTypeNames);
    midiType.setListener(this);

    // Binding number is the combo index where
    // zero means "any"
    juce::StringArray channelNames;
    channelNames.add("Any");
    for (int i = 1 ; i <= 16 ; i++) {
        channelNames.add(juce::String(i));
    }
    midiChannel.setItems(channelNames);
    midiChannel.setListener(this);

    triggerValue.setListener(this);

    // form fields are added during load()
    addChildComponent(triggerForm);

    qualifiers.add(&scope);
    qualifiers.add(&arguments);
    addAndMakeVisible(qualifiers);
}

void BindingForms::load(Provider* p, Binding* b)
{
    provider = p;

    juce::String prefix;
    Symbol* s = provider->getSymbols()->find(b->symbol);
    if (s == nullptr)
      prefix = "???: ";
    else if (s->functionProperties != nullptr)
      prefix = "Function: ";
    else if (s->parameterProperties != nullptr)
      prefix = "Parameter: ";
    else if (s->script != nullptr)
      prefix = "Script: ";
    else if (s->sample != nullptr)
      prefix = "Sample: ";
    else if (s->behavior ==  BehaviorActivation)
      prefix = "";
    else
      prefix = "???: ";
    
    title.setText(prefix + b->symbol, juce::NotificationType::dontSendNotification);

    // since the form is a member object and we reubild it every time,
    // it must be cleared first
    triggerForm.clear();

    if (b->trigger == Binding::TriggerUnknown) {
        Trace(1, "BindingForms: Trigger not set on binding");
    }
    else if (b->trigger == Binding::TriggerKey) {
        type = TypeKey;
    }
    else if (b->trigger == Binding::TriggerNote ||
             b->trigger == Binding::TriggerControl ||
             b->trigger == Binding::TriggerProgram) {
        type = TypeMidi;

        triggerForm.add(&midiType);
        triggerForm.add(&midiChannel);
    }
    else if (b->trigger == Binding::TriggerHost) {
        // nothing specific atm, maybe the unique parameter id?
        type = TypeHost;
    }
    else {
        Trace(1, "BindingForms: Unsupported trigger type %d", (int)(b->trigger));
    }

    if (type == TypeKey || type == TypeMidi) {
        triggerForm.add(&triggerValue);
        triggerForm.add(&release);
        triggerForm.add(&capture);
        captureText.setAdjacent(true);
        triggerForm.add(&captureText);
        
        triggerForm.setVisible(true);

        release.setValue(b->release);
        capture.setValue(false);
        captureText.setValue("");
    }

    if (type == TypeKey) {
        triggerValue.setValue(BindingUtil::renderTrigger(b));
    }
    else if (type == TypeMidi) {
        Binding::Trigger trigger = b->trigger;
        if (trigger == Binding::TriggerNote) {
            midiType.setSelection(0);
        }
        else if (trigger == Binding::TriggerControl) {
            midiType.setSelection(1);
        }
        else if (trigger == Binding::TriggerProgram) {
            midiType.setSelection(2);
        }
        else {
            // shouldn't be here, go back to Note
            Trace(1, "BindingForms: Invalide trigger type in MIDI form");
            midiType.setSelection(0);
        }
        
        midiChannel.setSelection(b->midiChannel);

        // todo: Capture is going to display symbolic names
        // but the value is a raw number, need one or the other
        // or both
        triggerValue.setValue(juce::String(b->triggerValue));
    }

    refreshScopeNames();
    refreshScopeValue(b);
    resized();
}

/**
 * This needs to be done every time the form is displayed in order
 * to track group renames.  Not an issue right now since this
 * entire component is rebuilt every time.
 */
void BindingForms::refreshScopeNames()
{
    // scope always goes first
    juce::StringArray scopeNames;
    scopeNames.add("Global");

    // context is not always set at this point so we have to go direct
    // to Supervisor to get to GroupDefinitions, this sucks work out a more
    // orderly initialization sequence

    MobiusView* view = provider->getMobiusView();
    maxTracks = view->totalTracks;
    for (int i = 0 ; i < maxTracks ; i++)
      scopeNames.add("Track " + juce::String(i+1));

    juce::StringArray gnames;
    GroupDefinitions* groups = provider->getGroupDefinitions();
    groups->getGroupNames(gnames);

    for (auto gname : gnames) {
        scopeNames.add(juce::String("Group ") + gname);
    }
    
    scope.setItems(scopeNames);
}

void BindingForms::refreshScopeValue(Binding* b)
{
    const char* scopeString = b->scope.toUTF8();
    int tracknum = Scope::parseTrackNumber(scopeString);
    if (tracknum > maxTracks) {
        // must be an old binding created before reducing
        // the track count, it reverts to global, should
        // have a more obvious warning in the UI
        Trace(1, "BindingForms: Binding scope track number out of range %d", tracknum);
    }
    else if (tracknum >= 0) {
        // element 0 is "global" so track number works
        scope.setSelection(tracknum);
    }
    else {
        GroupDefinitions* groups = provider->getGroupDefinitions();
        int index = groups->getGroupIndex(b->scope);
        if (index >= 0)
          scope.setSelection(maxTracks + index);
        else
          Trace(1, "BindingForms: Binding scope with unresolved group name %s", scopeString);
    }
}

void BindingForms::resized()
{
    juce::Rectangle<int> area = getLocalBounds();

    title.setBounds(area.removeFromTop(30));

    area.removeFromTop(8);

    if (type != TypeUnknown) {
    
        triggerTitle.setBounds(area.removeFromTop(30));
        area.removeFromTop(8);

        triggerForm.setBounds(area.removeFromTop(triggerForm.getPreferredHeight()));
        area.removeFromTop(8);
    }

    targetTitle.setBounds(area.removeFromTop(30));
    
    area.removeFromTop(8);

    qualifiers.setBounds(area.removeFromTop(qualifiers.getPreferredHeight()));
}

void BindingForms::yanInputChanged(YanInput* i)
{
    (void)i;

    // in the old binding editor it was important to dynamically track field changes
    // so they could be immediately reflected in the binding table, but now that this
    // is a popup over the table it doesn't matter
}

void BindingForms::yanComboSelected(YanCombo* c, int selection)
{
    (void)c;
    (void)selection;
}

//////////////////////////////////////////////////////////////////////
//
// Save
//
//////////////////////////////////////////////////////////////////////

void BindingForms::save(Binding* b)
{
    if (type == TypeMidi) {

        switch (midiType.getSelection()) {
            case 0: b->trigger = Binding::TriggerNote; break;
            case 1: b->trigger = Binding::TriggerControl; break;
            case 2: b->trigger = Binding::TriggerProgram; break;
            default: break;
        }

        b->midiChannel = midiChannel.getSelection();
        b->triggerValue = triggerValue.getInt();
    }
    else if (type == TypeKey) {
        b->triggerValue = unpackKeyCode();
    }

    b->scope = unpackScope();
    b->arguments = arguments.getValue();

    // only relevant for certain types
    if (b->trigger == Binding::TriggerKey || b->trigger == Binding::TriggerNote)
      b->release = release.getValue();
    else
      b->release = false;
}

/**
 * Undo the symbolic transformation to get back to
 * a raw key code
 */
int BindingForms::unpackKeyCode()
{
    juce::String value = triggerValue.getValue();
    int myCode = BindingUtil::unrenderKeyText(value);
    
    // capture has priority
    if (capture.getValue() && capturedCode > 0) {
        // test to see if there are any conditions where the text transform
        // doesn't end up  with the same thing
        if (capturedCode != myCode)
          Trace(1, "KeyboardEditor: Key encoding anomoly %d %d\n",
                capturedCode, myCode);
        myCode = capturedCode;
    }

    return myCode;
}

juce::String BindingForms::unpackScope()
{
    juce::String result;
    
    // item 0 is global, then tracks, then groups
    int item = scope.getSelection();
    if (item == 0) {
        // global
    }
    else if (item <= maxTracks) {
        // track number
        result = juce::String(item);
    }
    else {
        // skip going back to the SystemConfig for the names and
        // just remove our prefix
        juce::String itemName = scope.getSelectionText();
        juce::String groupName = itemName.fromFirstOccurrenceOf("Group ", false, false);
        result = groupName;
    }

    return result;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
