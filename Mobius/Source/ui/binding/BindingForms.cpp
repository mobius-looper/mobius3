
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
    
    //juce::StringArray triggerTypeNames;
    //triggerTypeNames.add("MIDI");
    //triggerTypeNames.add("Key");
    //triggerType.setItems(triggerTypeNames);
    //triggerType.setListener(this);
    //commonForm.add(&triggerType);
    //addAndMakeVisible(commonForm);

    juce::StringArray midiTypeNames;
    // could have an array of Triggers for these
    midiTypeNames.add("Note");
    midiTypeNames.add("Control");
    midiTypeNames.add("Program");
    midiType.setItems(midiTypeNames);
    midiType.setListener(this);
    midiForm.add(&midiType);

    // Binding number is the combo index where
    // zero means "any"
    juce::StringArray channelNames;
    channelNames.add("Any");
    for (int i = 1 ; i <= 16 ; i++) {
        channelNames.add(juce::String(i));
    }
    midiChannel.setItems(channelNames);
    midiChannel.setListener(this);
    midiForm.add(&midiChannel);

    midiValue.setListener(this);
    midiForm.add(&midiValue);
    
    midiForm.add(&midiRelease);
    midiForm.add(&midiCapture);
    midiForm.add(&midiSummary);
    addChildComponent(midiForm);

    keyForm.add(&keyValue);
    keyForm.add(&keyRelease);
    keyForm.add(&keyCapture);
    keyForm.add(&keySummary);
    addChildComponent(keyForm);

    qualifiers.add(&scope);
    qualifiers.add(&arguments);
    addAndMakeVisible(qualifiers);
}

void BindingForms::load(Provider* p, Binding* b)
{
    provider = p;
    showMidi = false;
    showKey = false;

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

    if (b->trigger == Binding::TriggerUnknown) {
        Trace(1, "BindingForms: Trigger not set on binding");
        // what to do now?
    }
    
    if (b->trigger == Binding::TriggerKey) {
        showKey = true;
    }
    else if (b->trigger == Binding::TriggerNote ||
             b->trigger == Binding::TriggerControl ||
             b->trigger == Binding::TriggerProgram) {
        showMidi = true;
    }
    else if (b->trigger == Binding::TriggerHost) {
        // nothing specific atm, maybe the unique parameter id?
    }
    else {
        Trace(1, "BindingForms: Unsupported trigger type %d", (int)(b->trigger));
    }

    if (showKey) {
        //triggerType.setSelection(1);

        keyForm.setVisible(true);
        midiForm.setVisible(false);
        
        keyValue.setValue(BindingUtil::renderTrigger(b));
        keyRelease.setValue(b->release);
        keyCapture.setValue(false);
        keySummary.setValue("");
    }
    else if (showMidi) {
        //triggerType.setSelection(0);
        keyForm.setVisible(false);
        midiForm.setVisible(true);

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
        midiValue.setValue(juce::String(b->triggerValue));
    }

    refreshScopeNames();
    refreshScopeValue(b);
    resized();
}

/**
 * This needs to be done every time in order to track group renames.
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

    triggerTitle.setBounds(area.removeFromTop(30));
    
    area.removeFromTop(8);

    //triggerType.setBounds(area.removeFromTop(12));

    if (midiForm.isVisible()) {
        midiForm.setBounds(area.removeFromTop(midiForm.getPreferredHeight()));
    }
    else if (keyForm.isVisible()) {
        keyForm.setBounds(area.removeFromTop(keyForm.getPreferredHeight()));
    }

    area.removeFromTop(8);

    targetTitle.setBounds(area.removeFromTop(30));
    
    area.removeFromTop(8);

    qualifiers.setBounds(area.removeFromTop(qualifiers.getPreferredHeight()));
}

void BindingForms::yanInputChanged(YanInput* i)
{
    (void)i;
    //formChanged();
}

void BindingForms::yanComboSelected(YanCombo* c, int selection)
{
    (void)c;
    (void)selection;
    //formChanged();
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
