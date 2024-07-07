/**
 * State for an interactive MSL session.
 *
 * Need to improve conveyance of errors.  To be useful the messages
 * need to have line/column numbers and it would support a syntax highlighter
 * better if the error would include that in an MslError model rather than
 * embedded in the text.
 */

#include <JuceHeader.h>

#include "MslSession.h"

/** 
 * Main entry point to parse and evaluate one script
 */
void MslSession::eval(juce::String src)
{
    MslNode* node = parser.parse(src);
    if (node == nullptr) {
        juce::StringArray* parseErrors = parser.getErrors();
        if (listener != nullptr) {
            for (auto error : *parseErrors)
              listener->mslError(error.toUTF8());
        }
    }
    else {

        // evaluation will run to the end unless there is a Wait
        // encountered
        // will need a way to pass back suspension state which means
        // the value here isn't relevant yet
        MslValue result = evaluator.start(this, node);

        juce::StringArray* evalErrors = evaluator.getErrors();
        if (listener != nullptr) {
            for (auto error : *evalErrors)
              listener->mslError(error.toUTF8());
        }
        
        const char* s = result.getString();
        if (s != nullptr) {
            if (listener != nullptr)
              listener->mslResult(s);
        }

        delete node;
    }
        
}

