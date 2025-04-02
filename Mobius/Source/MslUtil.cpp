
#include "util/Trace.h"

#include "model/Symbol.h"
#include "model/ParameterProperties.h"
#include "model/UIAction.h"

#include "script/MslValue.h"

#include "MslUtil.h"

/**
 * This little annoyance has to deal with the conversion of an MSL
 * assignment value passed as an MslValue into something that can
 * be conveyed in a UIAction.  The only thing UIAction supports
 * right now is an int, so symbolic enumeration names have to be
 * converted to ordinals.
 */
void MslUtil::mutateActionArgument(Symbol* s, MslValue* v, UIAction* a)
{
    if (v != nullptr) {
        if (s->functionProperties != nullptr) {
            // the most common function argument is "TrackSelect 1"
            // there are no functions that take random strings yet, but that won't last
            a->value = v->getInt();
        }
        else if (s->parameterProperties != nullptr) {
            // parameters need to support symbolic enumeration conversion
            switch (s->parameterProperties->type) {
                case TypeInt:
                case TypeBool:
                    a->value = v->getInt();
                    break;
                case TypeFloat: {
                    // only for transportTempo, this is expected as
                    // an x100 float from the UI, from MSL it can be either
                    // a float or an int but need to x100 either
                    if (v->type == MslValue::Int)
                      a->value = v->getInt() * 100;
                    else if (v->type == MslValue::Float)
                      a->value = (int)(v->getFloat() * 100.0f);
                    else {
                        // strings or what have you, cast to int
                        a->value = v->getInt() * 100;
                    }
                }
                    break;
                case TypeEnum: {
                    if (v->type == MslValue::Int) {
                        a->value = v->getInt();
                    }
                    else if (v->type == MslValue::String ||
                             v->type == MslValue::Keyword ||
                             v->type == MslValue::Enum) {
                        // don't trust the ordinal in the MslValue, which MSL shouldn't be
                        // setting anyway, just use the name
                        int ordinal = s->parameterProperties->getEnumOrdinal(v->getString());
                        if (ordinal < 0) {
                            Trace(1, "MslUtil: Invalid enumeration name for symbol %s %s",
                                  s->getName(), v->getString());
                            // can't prevent the action at this point, need a better way
                            // to raise errors
                        }
                        else
                          a->value = ordinal;
                    }
                    else {
                        // what you give me dude?
                        Trace(1, "MslUtil: MSL used invalid value type for an enum assignment");
                    }
                }
                    break;
                case TypeString: {
                    // can't act on string parameters through actions yet
                    Trace(1, "MslUtil: MSL attempted assignment of string parameter");
                }
                case TypeStructure: {
                    // this one is more complicated and requires access to
                    // ParameterSets for overlays and GroupDefinitions for groups
                    Trace(1, "MslUtil: MSL attempting to assign a Structure parameter");
                    // punt and assume ordinal, but this is almost certainly wrong
                    a->value = v->getInt();
                }
                    break;
            }
        }
    }
}

/**
 * Convert a query result that was the value of an enumerated parameter
 * into a pair of values to return to the interpreter.
 * Not liking this but it works.
 *
 * The compilcation here is access to the structure names.
 * 
 * ParameterHelper needs UIConfig, GroupDefinitions, and ParameterSets
 * MobiusKernel has GroupDefinitions and ParameterSets but it doesn't
 * have UIConfig.
 *
 * Funneling this through an abstract provider, but it's still broken.
 */
void MslUtil::mutateReturn(Provider* p, Symbol* s, int value, MslValue* retval)
{
    ParameterProperties* props = s->parameterProperties.get();
    if (props == nullptr) {
        // no extra definition, return whatever it was
        retval->setInt(value);
    }
    else {
        UIParameterType ptype = props->type;
        if (ptype == TypeEnum) {
            // don't use labels since I want scripters to get used to the names
            const char* ename = props->getEnumName(value);
            retval->setEnum(ename, value);
        }
        else if (ptype == TypeBool) {
            retval->setBool(value == 1);
        }
        else if (ptype == TypeStructure) {
            retval->setJString(p->getStructureName(s, value));
        }
        else {
            // should only be here for TypeInt
            // unclear what String would do
            retval->setInt(value);
        }
    }
}
