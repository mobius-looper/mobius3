
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/StaticConfig.h"
#include "../../model/TreeForm.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "ParameterForm.h"

#include "SessionFormCollection.h"

SessionFormCollection::SessionFormCollection()
{
}

void SessionFormCollection::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    for (auto form : forms)
      form->setBounds(area);
}

void SessionFormCollection::paint(juce::Graphics& g)
{
    g.setColour(juce::Colours::black);
    g.fillRect(0, 0, getWidth(), getHeight());
}

void SessionFormCollection::load(Provider* p, ValueSet* src)
{
    sourceValues = src;
    for (auto form : forms)
      form->load(p, sourceValues);
}

void SessionFormCollection::save(ValueSet* dest)
{
    for (auto form : forms)
      form->save(dest);
}

void SessionFormCollection::cancel()
{
    // forget this, just reload
    sourceValues = nullptr;
}

void SessionFormCollection::decache()
{
    // first save them
    if (sourceValues != nullptr)
      save(sourceValues);

    formTable.clear();
    forms.clear();
    currentForm = nullptr;
}

void SessionFormCollection::show(Provider* p, juce::String formName)
{
    ParameterForm* form = formTable[formName];

    if (form == nullptr) {

        StaticConfig* scon = p->getStaticConfig();
        TreeForm* formdef = scon->getForm(formName);
        if (formdef == nullptr) {
            Trace(1, "SessionFormCollection: Unknown form %s", formName.toUTF8());
        }
        else {
            //Trace(2, "SessionFormCollection: Creating form %s", formName.toUTF8());

            if (currentForm != nullptr)
              currentForm->setVisible(false);
        
            form = new ParameterForm();
            forms.add(form);
            // need more here!!
            formTable.set(formdef->name, form);
            addAndMakeVisible(form);

            juce::String title = formdef->title;
            if (title.length() == 0) title = formName;

            form->setTitle(title);
            form->add(p, formdef);

            // must have been loaded by now
            if (sourceValues != nullptr)
              form->load(p, sourceValues);
            else {
                // no, this will hit when we pre-select the tree element and form to be
                // showing when the editor is brought up for the first time, it simulates
                // a mouse click but nothing has actually been loaded yet
                //Trace(1, "SessionFormCollection: Attempt to show form without source values");
            }
            
            currentForm = form;

            form->setBounds(getLocalBounds());
            // trouble getting this fleshed out dynamically
            form->resized();
        }
    }
    else if (form == currentForm) {
        // Trace(2, "SPE: Form already displayed for category %s", category.toUTF8());
    }
    else {
        //Trace(2, "SessionFormCollection: Changing form %s", formName.toUTF8());
        currentForm->setVisible(false);
        // probably need a refresh?
        form->setVisible(true);
        currentForm = form;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
