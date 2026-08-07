#include "dump1090.h"

/* Write raw output to TCP clients. */
void modesSendRawOutput(struct modesMessage *mm) {
    char msg[128], *p = msg;
    int j;

    /* mm->msgbits/8 is at most MODES_LONG_MSG_BYTES (14), so the message
     * can never overflow this 128 byte buffer, but we still bound every
     * write explicitly so this stays true even if the message format
     * changes in the future. */
    *p++ = '*';
    for (j = 0; j < mm->msgbits/8 && (p - msg) < (int)sizeof(msg) - 3; j++) {
        snprintf(p, sizeof(msg) - (p - msg), "%02X", mm->msg[j]);
        p += 2;
    }
    *p++ = ';';
    *p++ = '\n';
    modesSendAllClients(Modes.ros, msg, p-msg);
}

/* Write SBS output to TCP clients. */
void modesSendSBSOutput(struct modesMessage *mm, struct aircraft *a) {
    char msg[256], *p = msg;
    int emergency = 0, ground = 0, alert = 0, spi = 0;

    if (mm->msgtype == 4 || mm->msgtype == 5 || mm->msgtype == 21) {
        /* Node: identity is calculated/kept in base10 but is actually
         * octal (07500 is represented as 7500) */
        if (mm->identity == 7500 || mm->identity == 7600 ||
            mm->identity == 7700) emergency = -1;
        if (mm->fs == 1 || mm->fs == 3) ground = -1;
        if (mm->fs == 2 || mm->fs == 3 || mm->fs == 4) alert = -1;
        if (mm->fs == 4 || mm->fs == 5) spi = -1;
    }

    /* All writes below are bounded with snprintf() against the remaining
     * space in msg[], and clamped to never advance p past the buffer, so
     * a future field (e.g. a longer flight number) can't overflow msg[]. */
#define SBS_REMAINING ((int)sizeof(msg) - (int)(p - msg))
#define SBS_ADVANCE(n) do { \
        int _n = (n); \
        if (_n > 0) p += (_n < SBS_REMAINING ? _n : SBS_REMAINING - 1); \
    } while (0)

    if (mm->msgtype == 0) {
        SBS_ADVANCE(snprintf(p, SBS_REMAINING, "MSG,5,,,%02X%02X%02X,,,,,,,%d,,,,,,,,,,",
        mm->aa1, mm->aa2, mm->aa3, mm->altitude));
    } else if (mm->msgtype == 4) {
        SBS_ADVANCE(snprintf(p, SBS_REMAINING, "MSG,5,,,%02X%02X%02X,,,,,,,%d,,,,,,,%d,%d,%d,%d",
        mm->aa1, mm->aa2, mm->aa3, mm->altitude, alert, emergency, spi, ground));
    } else if (mm->msgtype == 5) {
        SBS_ADVANCE(snprintf(p, SBS_REMAINING, "MSG,6,,,%02X%02X%02X,,,,,,,,,,,,,%d,%d,%d,%d,%d",
        mm->aa1, mm->aa2, mm->aa3, mm->identity, alert, emergency, spi, ground));
    } else if (mm->msgtype == 11) {
        SBS_ADVANCE(snprintf(p, SBS_REMAINING, "MSG,8,,,%02X%02X%02X,,,,,,,,,,,,,,,,,",
        mm->aa1, mm->aa2, mm->aa3));
    } else if ((mm->msgtype == 17 || mm->msgtype == 18) && mm->metype == 4) {
        SBS_ADVANCE(snprintf(p, SBS_REMAINING, "MSG,1,,,%02X%02X%02X,,,,,,%s,,,,,,,,0,0,0,0",
        mm->aa1, mm->aa2, mm->aa3, mm->flight));
    } else if ((mm->msgtype == 17 || mm->msgtype == 18) && mm->metype >= 9 && mm->metype <= 18) {
        if (a->lat == 0 && a->lon == 0)
            SBS_ADVANCE(snprintf(p, SBS_REMAINING, "MSG,3,,,%02X%02X%02X,,,,,,,%d,,,,,,,0,0,0,0",
            mm->aa1, mm->aa2, mm->aa3, mm->altitude));
        else
            SBS_ADVANCE(snprintf(p, SBS_REMAINING, "MSG,3,,,%02X%02X%02X,,,,,,,%d,,,%1.5f,%1.5f,,,"
                            "0,0,0,0",
            mm->aa1, mm->aa2, mm->aa3, mm->altitude, a->lat, a->lon));
    } else if ((mm->msgtype == 17 || mm->msgtype == 18) && mm->metype == 19 && mm->mesub == 1) {
        int vr = (mm->vert_rate_sign==0?1:-1) * (mm->vert_rate-1) * 64;

        SBS_ADVANCE(snprintf(p, SBS_REMAINING, "MSG,4,,,%02X%02X%02X,,,,,,,,%d,%d,,,%i,,0,0,0,0",
        mm->aa1, mm->aa2, mm->aa3, a->speed, a->track, vr));
    } else if (mm->msgtype == 21) {
        SBS_ADVANCE(snprintf(p, SBS_REMAINING, "MSG,6,,,%02X%02X%02X,,,,,,,,,,,,,%d,%d,%d,%d,%d",
        mm->aa1, mm->aa2, mm->aa3, mm->identity, alert, emergency, spi, ground));
    } else {
        return;
    }

#undef SBS_ADVANCE
#undef SBS_REMAINING

    if (p - msg < (int)sizeof(msg)) *p++ = '\n';
    modesSendAllClients(Modes.sbsos, msg, p-msg);
}

/* Turn an hex digit into its 4 bit decimal value.
 * Returns -1 if the digit is not in the 0-F range. */
int hexDigitVal(int c) {
    c = tolower(c);
    if (c >= '0' && c <= '9') return c-'0';
    else if (c >= 'a' && c <= 'f') return c-'a'+10;
    else return -1;
}

/* This function decodes a string representing a Mode S message in
 * raw hex format like: *8D4B969699155600E87406F5B69F;
 * The string is supposed to be at the start of the client buffer
 * and null-terminated.
 * 
 * The message is passed to the higher level layers, so it feeds
 * the selected screen output, the network output and so forth.
 * 
 * If the message looks invalid is silently discarded.
 *
 * The function always returns 0 (success) to the caller as there is
 * no case where we want broken messages here to close the client
 * connection. */
int decodeHexMessage(struct client *c) {
    char *hex = c->buf;
    int l = strlen(hex), j;
    unsigned char msg[MODES_LONG_MSG_BYTES];
    struct modesMessage mm;

    /* Remove spaces on the left and on the right. */
    while(l && isspace(hex[l-1])) {
        hex[l-1] = '\0';
        l--;
    }
    while(isspace(*hex)) {
        hex++;
        l--;
    }

    /* Turn the message into binary. */
    if (l < 2 || hex[0] != '*' || hex[l-1] != ';') return 0;
    hex++; l-=2; /* Skip * and ; */
    if (l > MODES_LONG_MSG_BYTES*2) return 0; /* Too long message... broken. */
    if (l % 2) return 0; /* Odd number of hex digits: malformed, and would
                           * otherwise read one byte past the message when
                           * decoding the last (incomplete) pair below. */
    for (j = 0; j < l; j += 2) {
        int high = hexDigitVal(hex[j]);
        int low = hexDigitVal(hex[j+1]);

        if (high == -1 || low == -1) return 0;
        msg[j/2] = (high<<4) | low;
    }
    decodeModesMessage(&mm,msg);
    useModesMessage(&mm);
    return 0;
}

