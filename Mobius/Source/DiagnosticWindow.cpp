
#include <JuceHeader.h>

#include "DiagnosticWindow.h"

DiagnosticWindowMain::DiagnosticWindowMain()
{
    setSize(500, 100);
}

DiagnosticWindowMain::~DiagnosticWindowMain()
{
}


DiagnosticWindow::DiagnosticWindow() :
    DocumentWindow (juce::String("Diagnostic Window"),
                    juce::Desktop::getInstance().getDefaultLookAndFeel()
                    .findColour (juce::ResizableWindow::backgroundColourId),
                    DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new DiagnosticWindowMain(), true);

    setResizable (true, true);
    
    //setSize(100, 100);
    centreWithSize (getWidth(), getHeight());

    setVisible (true);
}

DiagnosticWindow::~DiagnosticWindow()
{
}

void DiagnosticWindow::closeButtonPressed()
{
    delete this;
}


DiagnosticWindow* DiagnosticWindow::launch()
{
    DiagnosticWindow* win = new DiagnosticWindow();
    return win;
}
