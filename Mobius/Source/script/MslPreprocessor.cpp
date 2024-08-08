/**
 * Preprocessor for MSL source code prior to passing through the MslParser.
 *
 * The main thing this does is strip out /* block comments and
 * // end of line comments so the tokenizer and parser doesn't have to deal with them.
 * We'll also take the opportunity to parse and remove # directives.
 *
 * Note that when stripping block comments, it is important to leave newlines in the result
 * string for each block comment line so that the line numbers left behind by MslTokenizer
 * match the original source code.
 *
 */
 
#include <JuceHeader.h>

#include "MslPreprocessor.h"

juce::String MslPreprocessor::process(juce::String src)
{

    juce::String output;

    int psn = 0;
    while (psn < src.length()) {

        juce::juce_wchar ch = src[psn];

        if (ch == '/' && src[psn+1] == '/') {
            // eol comment
            psn += 2;
            while (psn < src.length()) {
                juce::juce_wchar ch2 = src[psn];
                psn++;
                if (ch2 == '\n') {
                    output += ch2;
                    break;
                }
            }
        }
        else if (ch == '/' && src[psn+1] == '*') {
            // block comment
            psn += 2;
            bool armed = false;
            while (psn < src.length()) {
                juce::juce_wchar ch2 = src[psn];
                psn++;
                if (ch2 == '*')
                  armed = true;
                else if (ch2 == '/' && armed)
                  break;
                else {
                    armed = false;
                    if (ch2 == '\n')
                      output += ch2;
                }
            }
        }
        else {
            // todo: handle # directives here or in the parser
            output += ch;
            psn++;
        }
    }

    return output;
}

                    
                
            
            
                                    
