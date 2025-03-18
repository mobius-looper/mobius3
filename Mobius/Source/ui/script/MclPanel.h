/**
 * A BasePanel extension that wraps MclConsole and gives it panelness.
 */

#pragma once

#include <JuceHeader.h>

#include "../BasePanel.h"
#include "MclConsole.h"

class MclPanel : public BasePanel
{
  public:

    MclPanel(class Supervisor* s) : content(s, this) {
        setTitle("MCL Console");
        setContent(&content);
        setSize(800, 500);
    }
    ~MclPanel() {}

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
    MclConsole content;
};
