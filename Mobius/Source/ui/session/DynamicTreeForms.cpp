 
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../model/Symbol.h"
#include "../../model/ValueSet.h"
#include "../../Provider.h"

#include "SymbolTree.h"
#include "DynamicParameterTree.h"
#include "DynamicFormCollection.h"

#include "DynamicTreeForms.h"

DynamicTreeForms::DynamicTreeForms()
{
    addAndMakeVisible(tree);
    addAndMakeVisible(forms);

    tree.setListener(this);
    tree.setDropListener(this);
    
    // set up the layout and resizer bars..
    verticalLayout.setItemLayout (0, -0.2, -0.8, -0.35); // width of the font list must be
    // between 20% and 80%, preferably 50%
    verticalLayout.setItemLayout (1, 8, 8, 8);           // the vertical divider drag-bar thing is always 8 pixels wide
    verticalLayout.setItemLayout (2, 150, -1.0, -0.65);  // the components on the right must be
    // at least 150 pixels wide, preferably 50% of the total width

    verticalDividerBar.reset (new juce::StretchableLayoutResizerBar (&verticalLayout, 1, true));
    addAndMakeVisible (verticalDividerBar.get());
}

DynamicTreeForms::~DynamicTreeForms()
{
}

/**
 * Initialize with the full symbol table
 */
void DynamicTreeForms::initialize(Provider* p)
{
    provider = p;

    tree.initialize(p);
    
    tree.selectFirst();
}

/**
 * Load values after initialization.
 */
void DynamicTreeForms::load(ValueSet* set)
{
    valueSet = set;
    forms.load(provider, set);
}

/**
 * Initialize with a restricted value set.
 *
 * Start initializing the tree just with things from the set,
 * but now it does a full tree so it can be used for drag-and-drop
 */
void DynamicTreeForms::initialize(Provider* p, ValueSet* set)
{
    provider = p;
    valueSet = set;

    // only show things in the ValueSet
    restricted = true;

    //tree.initialize(p, set);
    tree.initialize(p);

    // this actually won't do anything since the should be none during
    // initialization
    forms.load(p, set);

    // this is where the first form will be constructed
    tree.selectFirst();
}

void DynamicTreeForms::decache()
{
    forms.decache();
}

void DynamicTreeForms::save()
{
    save(valueSet);
}

void DynamicTreeForms::save(class ValueSet* set)
{
    forms.save(set);
}

void DynamicTreeForms::cancel()
{
    forms.cancel();
}

void DynamicTreeForms::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    
    bool withDivider = true;
    if (withDivider) {

        // lay out the list box and vertical divider..
        Component* vcomps[] = { &tree, verticalDividerBar.get(), nullptr };

        verticalLayout.layOutComponents (vcomps, 3,
                                         area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                                         false,     // lay out side-by-side
                                         true);     // resize the components' heights as well as widths


        area.removeFromLeft (verticalDividerBar->getRight());
        forms.setBounds(area);
     }
    else {
        tree.setBounds(area.removeFromLeft(100));
        forms.setBounds(area);
    }
}

void DynamicTreeForms::symbolTreeClicked(SymbolTreeItem* item)
{
    SymbolTreeItem* container = item;
    
    // if this is a leaf node, go up to the parent and show the entire parent form
    if (item->getNumSubItems() == 0) {
        container = static_cast<SymbolTreeItem*>(item->getParentItem());
    }

    // SymbolTreeItem is a generic model that doesn't understand it's usage
    // for form delivery, the tree builder left the form name as the "annotation"
    
    juce::String formName = container->getAnnotation();
    if (formName.length() > 0) {

        ParameterForm* form = forms.getForm(formName);
        if (form == nullptr) {
            form = buildForm(container);
            form->setListener(this);
            forms.addForm(formName, form);
        }
        forms.show(provider, formName);
    }
}

ParameterForm* DynamicTreeForms::buildForm(SymbolTreeItem* parent)
{
    ParameterForm* form = new ParameterForm();

    for (int i = 0 ; i < parent->getNumSubItems() ; i++) {
        SymbolTreeItem* item = static_cast<SymbolTreeItem*>(parent->getSubItem(i));
        juce::Array<Symbol*>& symbols = item->getSymbols();
        
        if (!restricted) {
            // add all of them under this tree node
            form->add(symbols);
        }
        else {
            // only those symbols that have matching values are shown
            for (auto s : symbols) {
                MslValue* v = valueSet->get(s->name);
                if (v != nullptr) {
                    // okay, so why does the incremental one need all this
                    // context but not the bulk symbol loader,
                    // work this out
                    form->add(provider, s, valueSet);
                }
            }
        }
            
    }
    
    return form;
}

//////////////////////////////////////////////////////////////////////
//
// Drag and Drop
//
//////////////////////////////////////////////////////////////////////

/**
 * Here in a roundabout way after a item was dragged from our DynamicParameterTree
 * onto one of the ParameterForms in our DynamicFormCollection.
 *
 * ParameterForm doesn't have enough context (the Provider) to alter itself, so it forwards
 * back to a higher power to do it.
 */
void DynamicTreeForms::parameterFormDrop(ParameterForm* src, juce::String desc)
{
    (void)src;

    // description is the display name, find the symbol
    Symbol* s = provider->getSymbols()->findDisplayName(desc);
    if (s == nullptr) {
        Trace(1, "DynamicTreeForms: Unable to locate symbol with display name %s",
              desc.toUTF8());
    }
    else {
        Trace(2, "DynamicTreeForms::parameterFormDrop %s", desc.toUTF8());

        // hmm, we don't necessarily need to pass the valueSet here since if this
        // is a new field, there shouldn't have been a value, but if they take it
        // out and put it back, this would restore the value
        src->add(provider, s, valueSet);
    }
}

void DynamicTreeForms::dropTreeViewDrop(DropTreeView* dtv, const juce::DragAndDropTarget::SourceDetails& details)
{
    (void)dtv;
    (void)details;

    juce::String oname = details.sourceComponent->getName();

    // god, target details sucks, a fucking String!?  That's it?
    // what we want is the ParameterForm the field is inside, we could traverse up
    // doing dynamic_cast until we find it, or walk down, it will normally
    // be the one that is currently displayed
    if (oname == "YanFieldLabel") {
        YanFieldLabel* label = dynamic_cast<YanFieldLabel*>(details.sourceComponent.get());
        if (label == nullptr) {
            Trace(1, "DynamicTreeForms: YanFieldLabel failed dynamic cast");
        }
        else {
            ParameterForm* form = forms.findFormWithLabel(label);
            if (form == nullptr) {
                Trace(1, "DynamicTreeForms: Unable to locate form with drag label");
            }
            else {
                // ugh, we just did this to find the ParameterForm
                // could avoid the redundant work by just asking to remove it when
                // it finds it...
                YanParameter* f = form->findFieldWithLabel(label);
                if (f == nullptr) {
                    Trace(1, "DynamicTreeForms: Form with label didn't have the field");
                }
                else {
                    form->remove(f);
                }
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
