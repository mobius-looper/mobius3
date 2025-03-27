#pragma once

#include "../config/ConfigPanel.h"
#include "BindingEditor.h"

class NewBindingPanel : public ConfigPanel
{
  public:
    NewBindingPanel(class Supervisor* s) : ConfigPanel(s), editor(s) {
        setName("NewBindingPanel");
        setEditor(&editor);
    }
    ~NewBindingPanel() {}
 
  private:
    BindingEditor editor;
};
