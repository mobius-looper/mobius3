
#include <JuceHeader.h>

#include "../ui/common/YanDialog.h"

#include "Task.h"
#include "TaskPromptDialog.h"

class DialogTestTask : public Task, public YanDialog::Listener
{
  public:
    
    DialogTestTask();

    void run() override;

    void cancel() override;
    
    // internal use but need to be public for the async notification?
    // void continueWorkflow(bool cancel);
    
    void yanDialogClosed(YanDialog* d, int button) override;

  private:

    TaskPromptDialog dialog {this};

    YanForm form;
    YanInput input {"Something"};

};


