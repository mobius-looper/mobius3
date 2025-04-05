#pragma once

#include "../config/ConfigPanel.h"
#include "ButtonsEditor.h"

class NewButtonPanel : public ConfigPanel
{
  public:
    NewButtonPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("NewButtonPanel");
        setEditor(&editor);
    }
    ~NewButtonPanel() {}
 
  private:
    ButtonsEditor editor;
};
