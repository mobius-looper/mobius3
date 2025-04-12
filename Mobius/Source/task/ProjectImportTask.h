
#include <JuceHeader.h>

#include "../ui/common/YanDialog.h"
#include "../mobius/TrackContent.h"

#include "Task.h"

class ProjectImportTask : public Task, public YanDialog::Listener
{
  public:
    
    ProjectImportTask();

    void run() override;

    void cancel();
    
    // internal use but need to be public for the async notification?
    // void continueWorkflow(bool cancel);
    
    void yanDialogClosed(YanDialog* d, int button);

  private:

    std::unique_ptr<class TrackContent> content;
    
    YanDialog resultDialog {this};

    // final file creation
    void compileProject();

};


