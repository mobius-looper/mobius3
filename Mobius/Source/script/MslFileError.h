/**
 * Represents all of the errors encountered while parsing one file.
 * The way the parser works now, there will only be one error, but that
 * may change and it might include errors detected during evaluation someday.
 *
 * Beyond the MslSerror details, this also holds the path of the file
 * and the source code of the script for display in the debug panel.
 *
 * Since the parser resides only in shell threads this object is NOT
 * pooled and may make use of juce::String
 *
 * These are created by ScriptClerk so are not really part of the core MSL model.
 *
 */
class MslFileErrors
{
  public:

    MslFileErrors() {}
    ~MslFileErrors() {}

    // file this came from
    juce::String path;

    // source code that was read
    juce::String code;

    // errors encountered
    // todo: since MslError is simple enough, this could just be an Array of objects
    // todo: might still want to just have a linked list here for consistency with
    // MslSession.  Need a conversion utility
    juce::OwnedArray<MslError> errors;

};

