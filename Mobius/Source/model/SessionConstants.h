/**
 * The session defines a large number of property names, most of which will
 * have a formal Symbol/ParameterProperties definition, but a few don't.
 *
 * For those that don't, keep name constants here to avoid fragile string literals
 * in the code.
 *
 * This is a hack to get new things added quickly, it is likely that most things
 * here actually do need formal Parameter definitions with Symbols.
 *
 */

// note that some of the transport symbols are stored in the session
// but are also actionable and will not be saved it changed at runtime

static const char* SessionTransportTempo = "transportTempo";
static const char* SessionTransportLength = "transportLength";
static const char* SessionTransportBeatsPerBar = "transortBeatsPerBar";
static const char* SessionTransportBarsPerLoop = "transportBarsPerLoop";
static const char* SessionTransportMidi = "transportMidi";
static const char* SessionTransportClocks = "transportClocks";
static const char* SessionTransportManualStart = "transportManualStart";
static const char* SessionTransportMinTempo = "transportMinTempo";
static const char* SessionTransportMaxTempo = "transportMaxTempo";
static const char* SessionTransportMetronome = "transportMetronome";

static const char* SessionHostBeatsPerBar = "hostBeatsPerBar";
static const char* SessionHostBarsPerLoop = "hostBarsPerLoop";
static const char* SessionHostOverride = "hostOverride";

static const char* SessionMidiBeatsPerBar = "midiBeatsPerBar";
static const char* SessionMidiBarsPerLoop = "midiBarsPerLoop";

