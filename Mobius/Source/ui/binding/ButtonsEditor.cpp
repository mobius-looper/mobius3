
#include "../../model/UIConfig.h"
#include "../../model/BindingSets.h"
#include "../../model/BindingSet.h"
#include "../../model/Binding.h"

#include "../../Supervisor.h"

#include "ButtonsEditor.h"

ButtonsEditor::ButtonsEditor(Supervisor* s) : BindingEditor(s)
{
}

void ButtonsEditor::load()
{
    UIConfig* uicon = supervisor->getUIConfig();
    BindingSets* converted = convert(uicon);
    install(converted, true);
}

void ButtonsEditor::save()
{
    juce::Array<ButtonSet*> sets;
    unconvert(bindingSets.get(), sets);

    UIConfig* uicon = supervisor->getUIConfig();
    uicon->buttonSets.clear();
    while (sets.size() > 0) {
        uicon->buttonSets.add(sets.removeAndReturn(0));
    }
    supervisor->updateUIConfig();

    // back down to BindingEditor
    cancel();
}

BindingSets* ButtonsEditor::convert(UIConfig* uicon)
{
    (void)uicon;
    BindingSets* sets = new BindingSets();

    for (auto buset : uicon->buttonSets) {
        BindingSet* biset = new BindingSet();
        biset->name = buset->name;
        sets->add(biset);

        for (auto button : buset->buttons) {
            Binding* b = new Binding();
            b->symbol = button->action;
            b->trigger = Binding::TriggerUI;
            b->displayName = button->name;
            b->scope = button->scope;
            b->arguments = button->arguments;
            b->color = button->color;
            biset->add(b);
        }
    }
    
    return sets;
}

void ButtonsEditor::unconvert(BindingSets* src, juce::Array<ButtonSet*>& dest)
{
    for (auto biset : src->getSets()) {
        ButtonSet* buset = new ButtonSet();
        buset->name = biset->name;
        dest.add(buset);

        for (auto b : biset->getBindings()) {

            DisplayButton* button = new DisplayButton();
            button->action = b->symbol;
            button->name = b->displayName;
            button->scope = b->scope;
            button->arguments = b->arguments;
            button->color = b->color;
            buset->buttons.add(button);
        }
    }
}
