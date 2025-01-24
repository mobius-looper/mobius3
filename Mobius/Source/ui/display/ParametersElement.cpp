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
#include "../../model/Preset.h"
#include "../../model/ParameterProperties.h"
#include "../../model/ParameterHelper.h"
#include "../../model/UIAction.h"
#include "../../model/Symbol.h"
#include "../../model/SymbolId.h"
#include "../../model/Query.h"

#include "../../Provider.h"
#include "../../Symbolizer.h"
#include "../../mobius/MobiusInterface.h"

#include "../MobiusView.h"
#include "../JuceUtil.h"
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
    area->getProvider()->addActionListener(this);

    // layout isn't responsive yet
    //resizes = true;
}

ParametersElement::~ParametersElement()
{
    statusArea->getProvider()->removeActionListener(this);
}

/**
 * To reduce flicker, retain the values of the currently displayed parameters
 * if they change position.
 *
 * With the introduction of script variables, scripts may not be loaded
 * at the time of initial configuration which is early in Supervisor::start.
 * But they will have ScroptProperties shortly after that.  The issue is similar
 * to ActionButtons that reference scripts that may or may not be loaded.  We still
 * display them, but they do nothing.  Might want to color the label red when this
 * happens so they know, but that will usually flicker on startup.  I suppose we could
 * defer that for a few refresh cycles, but actually since scripts can be unloaded
 * it can happen at any time, not just during startup.
 */
void ParametersElement::configure()
{
    UIConfig* config = statusArea->getProvider()->getUIConfig();

    // remember the parameter the cursor was currently on
    Symbol* current = nullptr;
    if (cursor < parameters.size())
      current = parameters[cursor]->symbol;

    // rebuild the parameter list, retaining the old values
    juce::Array<ParameterState*> newParameters;
    
    DisplayLayout* layout = config->getActiveLayout();
    for (auto name : layout->instantParameters) {

        // intern rather than find to pick up script symbols
        Symbol* s = statusArea->getProvider()->getSymbols()->intern(name);

        // rather than warning and ignoring if it doesn't resolve
        // always put what was configured on the list, just display it differently
        
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

    // do we really need to mess with UIParameter any more?
    // can't we just use parameterProperties
    if (s->parameter != nullptr) 
      displayName = s->parameter->getDisplayableName();
    
    else if (s->parameterProperties != nullptr)
      displayName = s->parameterProperties->displayName;

    else if (s->script != nullptr) {
        // exported variables don't have display names, seems find
    }
            
    if (displayName.length() == 0)
      displayName = s->name;

    return displayName;
}

int ParametersElement::getPreferredWidth()
{
    juce::Font font (JuceUtil::getFont(ParametersRowHeight));

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
 * to go back through Provider to get them in paint().
 */
void ParametersElement::update(MobiusView* view)
{
    bool changes = false;

    for (int i = 0 ; i < parameters.size() ; i++) {
        ParameterState* ps = parameters[i];
        int value = ps->value;

        // for unresolved symbols for script variables,
        // prevent Supervisor from logging about this and just
        // suppress the query
        if (isUnresolved(ps)) {
            // just keep the old value, which is usually zero
        }
        else {
            Query q (ps->symbol);
            // focusedTrack is zero based, Query scope is 1 based
            q.scope = view->focusedTrack + 1;
            if (statusArea->getProvider()->doQuery(&q))
              value = q.value;
            
            if (ps->value != value) {
                ps->value = value;
                changes = true;
            }
        }
    }
    
    if (changes)
      repaint();
}

bool ParametersElement::isUnresolved(ParameterState* ps)
{
    return (ps->symbol->parameter == nullptr &&
            ps->symbol->parameterProperties == nullptr &&
            ps->symbol->script == nullptr);
}

void ParametersElement::resized()
{
    // necessary to get the resizer
    StatusElement::resized();
}

void ParametersElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (isIdentify()) return;
    
    g.setFont(JuceUtil::getFont(ParametersRowHeight));

    int rowTop = 0;
    for (int i = 0 ; i < parameters.size() ; i++) {
        ParameterState* ps = parameters[i];
        Symbol* s = ps->symbol;
        int value = ps->value;
        juce::String strValue;

        // ugliness here due to the dual model again
        UIParameterType type = TypeInt;

        if (s->parameter != nullptr)
          type = s->parameter->type;
        else if (s->parameterProperties != nullptr)
          type = s->parameterProperties->type;
        else if (s->script != nullptr) {
            // these are interesting, MSL exports don't have a type, they
            // can be anything, they'll almost always be integers
            // but strings are possible
            // but since we're limited by what Query can return it
            // will always be an integer for now
        }
        
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
            strValue = ParameterHelper::getStructureName(statusArea->getProvider(), s, value);
        }
        else {
            strValue = juce::String(value);
        }

        // old mobius uses dim yellow
        if (isUnresolved(ps))
          g.setColour(juce::Colours::red);
        else
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
        
        case FuncParameterUp: {
            if (cursor > 0) {
                cursor--;
                repaint();
            }
            handled = true;
        }
            break;
            
        case FuncParameterDown: {
            if (cursor < (parameters.size() - 1)) {
                cursor++;
                repaint();
            }
            handled = true;
        }
            break;
            
        case FuncParameterInc: {
            ParameterState* ps = parameters[cursor];
            int value = ps->value;
            int max = getMax(ps);
            if (value < max ) {
                // update: coreAction is a misnomer
                UIAction coreAction;
                coreAction.symbol = ps->symbol;
                coreAction.value = value + 1;
                statusArea->getProvider()->doAction(&coreAction);
                // avoid refresh lag and flicker by optimistically setting the value
                // now and triggering an immediate repaint
                ps->value = coreAction.value;
                repaint();
            }
            handled = true;
        }
            break;
            
        case FuncParameterDec: {
            ParameterState* ps = parameters[cursor];
            int value = ps->value;
            // can assume that the minimum will always be zero
            if (value > 0) {
                UIAction coreAction;
                coreAction.symbol = ps->symbol;
                coreAction.value = value - 1;
                statusArea->getProvider()->doAction(&coreAction);
                ps->value = coreAction.value;
                repaint();
            }
            handled = true;
        }
            break;
        default: break;
    }
    
    return handled;
}

/**
 * With script variables there is no formal definition for them
 * so they won't have ranges, this is an interesting case, if you bother to export
 * a variable you generally want to interact with it, which means
 * the application needs to know its range and behavior
 * this needs thought
 * 
 * max is almost always parameter->high, but structure parameters
 * are variable and we have to query them
 */
int ParametersElement::getMax(ParameterState* ps)
{
    int max = 127;

    if (ps->symbol->parameterProperties != nullptr)
      max = ParameterHelper::getParameterMax(statusArea->getProvider(), ps->symbol);
         
    return max;
}

int ParametersElement::getMin(ParameterState* ps)
{
    int min = 0;

    // won't parameterProperties have this too?
    if (ps->symbol->parameter != nullptr)
      min = ps->symbol->parameter->low;

    return min;
}

/**
 * Within this element, clicking over a title activates the element drag
 * and clicking over a value activates the parameter row and allows value drag.
 */
void ParametersElement::mouseDown(const juce::MouseEvent& e)
{
    if (e.getMouseDownX() < maxNameWidth) {
        // in the label area, let it drag
        StatusElement::mouseDown(e);
    }
    else {
        int row = e.getMouseDownY() / (ParametersRowHeight + ParametersVerticalGap);
        //Trace(2, "Parameter row %d", row);
        cursor = row;
        repaint();

        // if value drag is enabled, remember where we started
        valueDrag = true;

        ParameterState* ps = parameters[row];
        valueDragStart = ps->value;

        // most have a min of zero, Subcycles is an outlier with a min of 1
        valueDragMin = getMin(ps);
        valueDragMax = getMax(ps);
    }
}

/**
 * Here we try to replicate the old drag-value behavior.
 * There are lots of ways to do this, but the expectation is that if the mouse moves
 * up or to the right, the value increases.
 *
 * So this can be controlled without jitter the distance from the down point needs to e
 * quantized into "units", let's start with 10 pixels.
 *
 * What these units do could be interpreted several ways.  One is to treat each unit
 * as a value increment/decrement so if you're one unit away the value goes from the
 * original value to original+1, if you move two units away original+2.
 * The space around the mouse down represents a grid of possible value as you move into
 * it the parameter is assigned that value.
 *
 * Another way is just to watch relative mouse movements, old Mobius tried to do that
 * but I forget exactly how it worked, you could "spin" and it would try to act like
 * a knob.
 *
 * I now think it's more predictable if you visualze a space around the mouse that looks
 * like this
 *
 *        2
 *        1
 *  -2 -1 X 1 2 3
 *        -1
 *        -2
 *
 * Each "square" around the X represents an increment or decrement, and if you want
 * both axes to count, then the max of the two of them wins.  So if you first go to the right
 * into X+1 space, but then move up into Y+2 space, the increment will be 2.
 *
 * Not sure I want to get the X axis involved, the most obvious is to think of it like
 * scrolling, you scroll up and down in the Y axis.
 *
 */
void ParametersElement::mouseDrag(const juce::MouseEvent& e)
{
    if (!valueDrag && e.getMouseDownX() < maxNameWidth)
      StatusElement::mouseDrag(e);
    else {
        juce::Point<int> offset = e.getOffsetFromDragStart();
        int unitsY = offset.getY() / 10;

        // invert Y, pushing "up" means increment
        int newValue = valueDragStart - unitsY;
        if (newValue < valueDragMin) newValue = valueDragMin;
        else if (newValue > valueDragMax) newValue = valueDragMax;

        ParameterState* ps = parameters[cursor];
        int value = ps->value;
        
        if (newValue != value) {
            //Trace(2, "Changing value to %d", newValue);
            UIAction coreAction;
            coreAction.symbol = ps->symbol;
            coreAction.value = newValue;
            statusArea->getProvider()->doAction(&coreAction);
            // avoid refresh lag and flicker by optimistically setting the value
            // now and triggering an immediate repaint
            ps->value = coreAction.value;
            repaint();
        }
    }
}

void ParametersElement::mouseUp(const juce::MouseEvent& e)
{
    if (e.getMouseDownX() < maxNameWidth)
      StatusElement::mouseUp(e);

    // wherever it is, it cancels value drag
    valueDrag = false;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

