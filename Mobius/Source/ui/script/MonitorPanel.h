/**
 * A BasePanel extension that gives ScriptMonitor panelness.
 */

#pragma once

#include <JuceHeader.h>

#include "../BasePanel.h"
#include "ScriptMonitor.h"

class MonitorPanel : public BasePanel
{
  public:

    MonitorPanel(class Supervisor* s) : content(s, this) {
        setTitle("Script Monitor");
        setContent(&content);
        setSize(800, 500);
    }
    ~MonitorPanel() {}

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
    ScriptMonitor content;
};
