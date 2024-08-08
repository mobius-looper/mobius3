/**
 * Simple panel that shows information about what this is 
 * and who we are.
 */

#include <JuceHeader.h>

#include "../Supervisor.h"

#include "BasePanel.h"
#include "AboutPanel.h"

AboutContent::AboutContent(Supervisor* s)
{
    supervisor = s;
    // yes, folks, that mess is an umlat-o
    // see here why this is complicated
    // https://forum.juce.com/t/embedding-unicode-string-literals-in-your-cpp-files/12600    
    product.setText(juce::CharPointer_UTF8("M\xc3\xb6""bius 3 by"), juce::NotificationType::dontSendNotification);
    addAndMakeVisible(product);

    url = juce::URL("http://www.circularlabs.com");
    hyper.setButtonText("Circular Labs");
    hyper.setURL(url);
    addAndMakeVisible(hyper);

    // same with copyright symbol
    copyright.setText(juce::CharPointer_UTF8("\xc2\xa9 Jeffrey Larson"), juce::NotificationType::dontSendNotification);
    addAndMakeVisible(copyright);
        
    juce::String buildText ("Build: ");
    buildText += juce::String(Supervisor::BuildNumber);
    build.setText(buildText, juce::NotificationType::dontSendNotification);
    addAndMakeVisible(build);
    
    juce::String rootText ("Configuration root: ");
    rootText += supervisor->getRoot().getFullPathName();
    root.setText(rootText, juce::NotificationType::dontSendNotification);
    addAndMakeVisible(root);
}

void AboutContent::resized()
{
    juce::Rectangle area = getLocalBounds();

    area.removeFromTop(10);
    juce::Rectangle productArea = area.removeFromTop(18);

    juce::Font font ((float)(productArea.getHeight()));
    int productWidth = font.getStringWidth(product.getText());
    // there is still a gap between the product text and the hyperlink
    // not sure if the getStringWidth is inaccurate, or if there
    // is something weird with hyperlink rendering, whatever it's close enough
    productWidth -= 12;
    product.setBounds(productArea.removeFromLeft(productWidth));
    hyper.setBounds(productArea);
    hyper.changeWidthToFitText();

    build.setBounds(area.removeFromTop(18));
    root.setBounds(area.removeFromTop(18));

    area.removeFromBottom(10);
    copyright.setBounds(area.removeFromBottom(18));
}
