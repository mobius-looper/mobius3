/**
 * Represents a uniquely named object that can be referenced within the
 * script environment and/or the outside application.
 *
 * Maintained within an MslResolutionContext.
 * This is conceptually similar to the Symbols table in the Mobius application.
 *
 */
class MslLinkage
{
  public:

    MslLinkage() {}
    ~MslLinkage() {}

    // the name that can be referenced by an MslSymbol
    juce::String name;

    // the compilation unit this came from
    class MslCompilation* unit = nullptr;

    // the resolved target of the link
    class MslFunction* function =  nullptr;
    class MslVariableExport* variable = nullptr;
    
};

