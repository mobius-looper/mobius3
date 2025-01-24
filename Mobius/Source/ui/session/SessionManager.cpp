
#include <JuceHeader.h>

#include "../../util/Trace.h"
#include "../../util/Util.h"
#include "../../Supervisor.h"
#include "../JuceUtil.h"

#include "SessionManagerPanel.h"
#include "SessionManager.h"

SessionManager::SessionManager(Supervisor* s, SessionManagerPanel* parent) :
    sessions(s)
{
    supervisor = s;
    panel = parent;
    addAndMakeVisible(sessions);
}

SessionManager::~SessionManager()
{
}

void SessionManager::showing()
{
    sessions.load();
}

void SessionManager::hiding()
{
}

void SessionManager::resized()
{
    juce::Rectangle<int> area = getLocalBounds();
    sessions.setBounds(area);
}

void SessionManager::paint(juce::Graphics& g)
{
    (void)g;
}

/**
 * Called during Supervisor's advance() in the maintenance thread.
 * Also using theh juce::Timer so don't need both
 */
void SessionManager::update()
{
}

