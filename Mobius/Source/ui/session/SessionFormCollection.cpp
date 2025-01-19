
#include <JuceHeader.h>

#include "../../util/Trace.h"

#include "../../model/SystemConfig.h"
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
    g.setColour(juce::Colours::pink);
    g.fillRect(0, 0, getWidth(), getHeight());
}

void SessionFormCollection::load(ValueSet* src)
{
    sourceValues = src;
    for (auto form : forms)
      form->load(sourceValues);
}

void SessionFormCollection::save(ValueSet* dest)
{
    for (auto form : forms)
      form->save(dest);
}

void SessionFormCollection::show(Provider* p, juce::String formName)
{
    ParameterForm* form = formTable[formName];

    if (form == nullptr) {

        SystemConfig* scon = p->getSystemConfig();
        TreeForm* formdef = scon->getForm(formName);
        if (formdef == nullptr) {
            Trace(1, "SessionFormCollection: Unknown form %s", formName.toUTF8());
        }
        else {
            Trace(2, "SessionFormCollection: Creating form %s", formName.toUTF8());
        
            form = new ParameterForm();
            forms.add(form);
            // need more here!!
            formTable.set(formdef->name, form);
            addAndMakeVisible(form);

            form->setTitle(formName);
            form->add(formdef);
            form->load(sourceValues);
            
            currentForm = form;
            currentForm->load(sourceValues);

            form->setBounds(getLocalBounds());
            // trouble getting this fleshed out dynamically
            form->resized();
        }
    }
    else if (form == currentForm) {
        // Trace(2, "SPE: Form already displayed for category %s", category.toUTF8());
    }
    else {
        Trace(2, "SessionFormCollection: Changing form %s", formName.toUTF8());
        currentForm->setVisible(false);
        // probably need a refresh?
        form->setVisible(true);
        currentForm = form;
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
