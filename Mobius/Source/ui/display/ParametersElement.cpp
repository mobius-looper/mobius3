/**
 * Status element to display a configued set of Parameter values
 * and allow temporary editing outside of a full edit of the MobiusConfig.
 *
 * The parameter values are displayed for the selected track.  With standard
 * bindings, the up/down arrows move the cursor between parameters and the
 * left/right arrows cycle through possible values.  Best when used with
 * enumerated values.
 *
 */


#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/List.h"
#include "../../model/UIConfig.h"
#include "../../model/MobiusConfig.h"
#include "../../model/Preset.h"
#include "../../model/MobiusState.h"
#include "../../model/UIParameter.h"
#include "../../model/ParameterProperties.h"
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../../model/Query.h"

#include "../../Supervisor.h"
#include "../../UISymbols.h"
#include "../../mobius/MobiusInterface.h"

#include "Colors.h"
#include "StatusArea.h"

#include "ParametersElement.h"

const int ParametersRowHeight = 20;
const int ParametersVerticalGap = 1;
const int ParametersValueWidth = 100;
const int ParametersHorizontalGap = 4;

ParametersElement::ParametersElement(StatusArea* area) :
    StatusElement(area, "ParametersElement")
{
    // intercept our cursor actions
    Supervisor* s = Supervisor::Instance;
    s->addActionListener(this);
}

ParametersElement::~ParametersElement()
{
    Supervisor* s = Supervisor::Instance;
    s->removeActionListener(this);
}

/**
 * To reduce flicker, retain the values of the currently displayed parameters
 * if they change position.
 *
 * update: Once UI level parameters stopped being UIParameters, we lost the
 * ability to 
 */
void ParametersElement::configure()
{
    UIConfig* config = Supervisor::Instance->getUIConfig();

    // remember the parameter the cursor was currently on
    Symbol* current = nullptr;
    if (cursor < parameters.size())
      current = parameters[cursor]->symbol;

    // rebuild the parameter list, retaining the old values
    juce::Array<ParameterState*> newParameters;
    
    DisplayLayout* layout = config->getActiveLayout();
    for (auto name : layout->instantParameters) {
        
        Symbol* s = Symbols.find(name);
        if (s == nullptr ||
            // two ways to represent these now...
            (s->parameter == nullptr &&
             s->parameterProperties == nullptr)) {

            Trace(1, "ParametersElement: Invalid parameter name %s\n", name.toUTF8());
        }
        else {
            ParameterState* existing = nullptr;
            for (auto pstate : parameters) {
                if (pstate->symbol == s) {
                    existing = pstate;
                    break;
                }
            }
            if (existing != nullptr) {
                parameters.removeObject(existing, false);
                newParameters.add(existing);
            }
            else {
                ParameterState* neu = new ParameterState();
                neu->symbol = s;
                newParameters.add(neu);
            }
        }
    }

    // what remains was removed from the display list, deleete them
    parameters.clear();

    // put the previous ones back with any new ones added
    // and try to make the cursor follow the previous parameter it was over
    cursor = 0;
    int position = 0;
    for (auto ps : newParameters) {
        parameters.add(ps);
        if (current != nullptr && current == ps->symbol)
          cursor = position;
        position++;
    }
}

int ParametersElement::getPreferredHeight()
{
    return (ParametersRowHeight + ParametersVerticalGap) * parameters.size();
}

/**
 * Derive the display name for a parameter symbol.
 * Temporary complication due to the evoluation from UIParameter
 * to ParameterProperties
 */
juce::String ParametersElement::getDisplayName(Symbol* s)
{
    juce::String displayName;
    
    if (s->parameter != nullptr) 
      displayName = s->parameter->getDisplayableName();
    
    else
      displayName = s->parameterProperties->displayName;
            
    if (displayName.length() == 0)
      displayName = s->name;

    return displayName;
}

int ParametersElement::getPreferredWidth()
{
    juce::Font font = juce::Font(ParametersRowHeight);

    int maxName = 0;
    for (int i = 0 ; i < parameters.size() ; i++) {
        juce::String displayName = getDisplayName(parameters[i]->symbol);
        int nameWidth = font.getStringWidth(displayName);
        if (nameWidth > maxName)
          maxName = nameWidth;
    }

    // remember this for paint
    // StatusArea must resize after configure() is called
    maxNameWidth = maxName;
    
    // width of parmeter values is relatively constrained, the exception
    // being preset names.  For enumerated values, assume our static size is enough
    // but could be smarter.  Would require iteration of all possible enumeration
    // values which we don't have yet
    // gag this is ugly, punt and pick a string about as long as usual, can squash
    // the actual values when painted
    maxValueWidth = font.getStringWidth(juce::String("MMMMMMMMMMMM"));
    
    return maxNameWidth + ParametersHorizontalGap + maxValueWidth;
}

/**
 * Save the values of the parameters for display.
 * Since we save them for difference detection we also don't need
 * to go back through Supervisor to get them in paint().
 */
void ParametersElement::update(MobiusState* state)
{
    bool changes = false;
    
    for (int i = 0 ; i < parameters.size() ; i++) {
        ParameterState* ps = parameters[i];
        // don't need this any more, can use the symbol
        // UIParameter* p = ps->parameter;
        int value = ps->value;
        Query q (ps->symbol);
        q.scope = state->activeTrack;
        if (Supervisor::Instance->doQuery(&q))
          value = q.value;
            
        if (ps->value != value) {
            ps->value = value;
            changes = true;
        }
    }

    if (changes)
      repaint();
}

void ParametersElement::resized()
{
}

void ParametersElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    g.setFont(juce::Font(ParametersRowHeight));

    int rowTop = 0;
    for (int i = 0 ; i < parameters.size() ; i++) {
        ParameterState* ps = parameters[i];
        Symbol* s = ps->symbol;
        int value = ps->value;
        juce::String strValue;

        // ugliness here due to the dual model again
        UIParameterType type;

        if (s->parameter != nullptr)
          type = s->parameter->type;
        else
          type = s->parameterProperties->type;
        
        if (type == TypeEnum) {
            // only works for UIParameter
            UIParameter* p = s->parameter;
            if (p != nullptr)
              strValue = juce::String(p->getEnumLabel(value));
        }
        else if (type == TypeBool) {
            if (value)
              strValue = juce::String("true");
            else
              strValue = juce::String("false");
        }
        else if (type == TypeStructure) {
            strValue = Supervisor::Instance->getParameterLabel(s, value);
        }
        else {
            strValue = juce::String(value);
        }

        // old mobius uses dim yellow
        g.setColour(juce::Colours::beige);
        g.drawText(getDisplayName(s),
                   0, rowTop, maxNameWidth, ParametersRowHeight,
                   juce::Justification::centredRight);

        if (i == cursor) {
            g.setColour(juce::Colours::white);
            g.drawRect(maxNameWidth + ParametersHorizontalGap, rowTop,
                       ParametersValueWidth, ParametersRowHeight);
        }

        g.setColour(juce::Colours::yellow);
        bool fitted = true;
        int textLeft = maxNameWidth + ParametersHorizontalGap;
        if (fitted)
          g.drawFittedText(strValue, textLeft, rowTop, 
                           ParametersValueWidth, ParametersRowHeight,
                           juce::Justification::centredLeft, 1);
        else
          g.drawText(strValue, textLeft, rowTop,
                     ParametersValueWidth, ParametersRowHeight,
                     juce::Justification::centredLeft);

        rowTop = rowTop + ParametersRowHeight + ParametersVerticalGap;
    }
}

/**
 * Cursor actions
 */
bool ParametersElement::doAction(UIAction* action)
{
    bool handled = false;

    switch (action->symbol->id) {
        
        case UISymbolParameterUp: {
            if (cursor > 0) {
                cursor--;
                repaint();
            }
            handled = true;
        }
            break;
            
        case UISymbolParameterDown: {
            if (cursor < (parameters.size() - 1)) {
                cursor++;
                repaint();
            }
            handled = true;
        }
            break;
            
        case UISymbolParameterInc: {
            ParameterState* ps = parameters[cursor];
            int value = ps->value;
            int max = Supervisor::Instance->getParameterMax(ps->symbol);
            if (value < max ) {
                UIAction coreAction;
                coreAction.symbol = ps->symbol;
                coreAction.value = value + 1;
                Supervisor::Instance->doAction(&coreAction);
                // avoid refresh lag and flicker by optimistically setting the value
                // now and triggering an immediate repaint
                ps->value = coreAction.value;
                repaint();
            }
            handled = true;
        }
            break;
            
        case UISymbolParameterDec: {
            ParameterState* ps = parameters[cursor];
            int value = ps->value;
            // can assume that the minimum will always be zero
            if (value > 0) {
                UIAction coreAction;
                coreAction.symbol = ps->symbol;
                coreAction.value = value - 1;
                Supervisor::Instance->doAction(&coreAction);
                ps->value = coreAction.value;
                repaint();
            }
            handled = true;
        }
            break;
    }
    
    return handled;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

