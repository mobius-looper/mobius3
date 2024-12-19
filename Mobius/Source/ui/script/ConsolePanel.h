/**
 * A BasePanel extension that wraps MobiusConsole and gives it panelness.
 */

#pragma once

#include <JuceHeader.h>

#include "../BasePanel.h"
#include "MobiusConsole.h"

class ConsolePanel : public BasePanel
{
  public:

    ConsolePanel(class Supervisor* s) : content(s, this) {
        setTitle("Mobius Console");
        setContent(&content);
        setSize(800, 500);
    }
    ~ConsolePanel() {}

    void update() override {
        content.update();
    }

    void showing() override {
        content.showing();
    }
    
    void hiding() override {
        content.hiding();
    }
    
  private:
    MobiusConsole content;
};
