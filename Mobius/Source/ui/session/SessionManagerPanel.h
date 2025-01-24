/**
 * A BasePanel extension that gives SessionManager panelness.
 */

#pragma once

#include <JuceHeader.h>

#include "../BasePanel.h"
#include "SessionManager.h"

class SessionManagerPanel : public BasePanel
{
  public:

    SessionManagerPanel(class Supervisor* s) : content(s, this) {
        setTitle("Session Manager");
        setContent(&content);
        setSize(800, 500);
    }
    ~SessionManagerPanel() {}

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
    SessionManager content;
};
