

#include <JuceHeader.h>

#include "../../model/MobiusState.h"
#include "../../model/UIConfig.h"

#include "../../Supervisor.h"
#include "../JuceUtil.h"

#include "Colors.h"
#include "StatusArea.h"
#include "AlertElement.h"

AlertElement::AlertElement(StatusArea* area) :
    StatusElement(area, "AlertElement")
{
    mouseEnterIdentify = true;
    area->getSupervisor()->addAlertListener(this);
    resizes = true;
}

AlertElement::~AlertElement()
{
    statusArea->getSupervisor()->removeAlertListener(this);
}

void AlertElement::configure()
{
    UIConfig* config = statusArea->getSupervisor()->getUIConfig();
    //alertHeight = config->getInt("alertHeight");
    alertDuration = config->getInt("alertDuration");
}

/**
 * AlertListener
 */
void AlertElement::alertReceived(juce::String msg)
{
    alert = msg;

    // timeout is in tenths to match the maintenance thread interval
    // should be using a millisecond counter
    // let the visible parameter be specified in even seconds
    // twmm wants to allow this to be large, a day should be enough
    if (alertDuration > 0 && alertDuration < 86400) {
        timeout = alertDuration * 10;
    }
    else {
        // default 5 seconds
        timeout = 50;
    }
    
    repaint();
}

/**
 * There is nothing in MobiusState that we need to watch
 * but we will use the udpate call to implement the timeout.
 * Assumig every 1/10th second
 */
void AlertElement::update(MobiusState* state)
{
    (void)state;
    if (timeout > 0) {
        timeout = timeout - 1;
        if (timeout == 0) {
            alert.clear();
            repaint();
        }
    }
}

int AlertElement::getPreferredHeight()
{
    // unclear if configure() happens before this, get it directly from UIConfig
    UIConfig* config = statusArea->getSupervisor()->getUIConfig();
    int alertHeight = config->getInt("alertHeight");
    if (alertHeight < 20 || alertHeight > 100)
      alertHeight = 20;
    
    return alertHeight;
}

int AlertElement::getPreferredWidth()
{
    // this should be proportional to height
    return 400;
}

void AlertElement::resized()
{
    // necessary to get the resizer
    StatusElement::resized();
}

void AlertElement::paint(juce::Graphics& g)
{
    // borders, labels, etc.
    StatusElement::paint(g);
    if (!isIdentify()) {
        g.setColour(juce::Colour(MobiusBlue));
        juce::Font font (JuceUtil::getFontf(getHeight() * 0.8f));
        g.setFont(font);
        g.drawText(alert, 0, 0, getWidth(), getHeight(), juce::Justification::left);
    }
}
