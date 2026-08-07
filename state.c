/* state.c -- definition (storage) for the program's shared global state.
 *
 * Every other module only ever sees `extern struct modes_state Modes;` and
 * `extern struct errorinfo bitErrorTable[];` (declared in dump1090.h) and
 * links against the single definition here. Keeping the definition in one
 * place makes it obvious where the "shared mutable state" of the program
 * lives -- which is also exactly the thing a future refactor should try to
 * shrink. */
#include "dump1090.h"

struct modes_state Modes;
struct errorinfo bitErrorTable[NERRORINFO];

/* Takes the address of fields inside Modes, so it must be defined here,
 * after Modes itself, rather than in net/server.c. */
struct net_service modesNetServices[MODES_NET_SERVICES_NUM] = {
    {"Raw TCP output", &Modes.ros, MODES_NET_OUTPUT_RAW_PORT},
    {"Raw TCP input", &Modes.ris, MODES_NET_INPUT_RAW_PORT},
    {"HTTP server", &Modes.https, MODES_NET_HTTP_PORT},
    {"Basestation TCP output", &Modes.sbsos, MODES_NET_OUTPUT_SBS_PORT}
};
