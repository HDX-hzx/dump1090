#include "dump1090.h"

/* When a new message is available, because it was decoded from the
 * RTL device, file, or received in the TCP input port, or any other
 * way we can receive a decoded message, we call this function in order
 * to use the message.
 *
 * Basically this function passes a raw message to the upper layers for
 * further processing and visualization. */
void useModesMessage(struct modesMessage *mm) {
    if (!Modes.stats && (Modes.check_crc == 0 || mm->crcok)) {
        /* Track aircrafts in interactive mode or if the HTTP
         * interface is enabled. */
        if (Modes.interactive || Modes.stat_http_requests > 0 || Modes.stat_sbs_connections > 0) {
            struct aircraft *a = interactiveReceiveData(mm);
            if (a && Modes.stat_sbs_connections > 0) modesSendSBSOutput(mm, a);  /* Feed SBS output clients. */
        }
        /* In non-interactive way, display messages on standard output. */
        if (!Modes.interactive) {
            displayModesMessage(mm);
            if (!Modes.raw && !Modes.onlyaddr) printf("\n");
        }
        /* Send data to connected clients. */
        if (Modes.net) {
            modesSendRawOutput(mm);  /* Feed raw output clients. */
        }
    }
}

/* ========================= Interactive mode =============================== */

/* Return a new aircraft structure for the interactive mode linked list
 * of aircrafts. */
struct aircraft *interactiveCreateAircraft(uint32_t addr) {
    struct aircraft *a = malloc(sizeof(*a));

    if (a == NULL) {
        fprintf(stderr, "Out of memory allocating a new aircraft entry.\n");
        exit(1);
    }

    a->addr = addr;
    snprintf(a->hexaddr,sizeof(a->hexaddr),"%06x",(int)addr);
    a->flight[0] = '\0';
    a->altitude = 0;
    a->speed = 0;
    a->track = 0;
    a->odd_cprlat = 0;
    a->odd_cprlon = 0;
    a->odd_cprtime = 0;
    a->even_cprlat = 0;
    a->even_cprlon = 0;
    a->even_cprtime = 0;
    a->lat = 0;
    a->lon = 0;
    a->seen = time(NULL);
    a->messages = 0;
    a->next = NULL;
    return a;
}

/* Return the aircraft with the specified address, or NULL if no aircraft
 * exists with this address. */
struct aircraft *interactiveFindAircraft(uint32_t addr) {
    struct aircraft *a = Modes.aircrafts;

    while(a) {
        if (a->addr == addr) return a;
        a = a->next;
    }
    return NULL;
}

/* Receive new messages and populate the interactive mode with more info. */
struct aircraft *interactiveReceiveData(struct modesMessage *mm) {
    uint32_t addr;
    struct aircraft *a, *aux;

    if (Modes.check_crc && mm->crcok == 0) return NULL;
    addr = (mm->aa1 << 16) | (mm->aa2 << 8) | mm->aa3;

    /* Loookup our aircraft or create a new one. */
    a = interactiveFindAircraft(addr);
    if (!a) {
        a = interactiveCreateAircraft(addr);
        a->next = Modes.aircrafts;
        Modes.aircrafts = a;
    } else {
        /* If it is an already known aircraft, move it on head
         * so we keep aircrafts ordered by received message time.
         *
         * However move it on head only if at least one second elapsed
         * since the aircraft that is currently on head sent a message,
         * othewise with multiple aircrafts at the same time we have an
         * useless shuffle of positions on the screen. */
        if (0 && Modes.aircrafts != a && (time(NULL) - a->seen) >= 1) {
            aux = Modes.aircrafts;
            while(aux->next != a) aux = aux->next;
            /* Now we are a node before the aircraft to remove. */
            aux->next = aux->next->next; /* removed. */
            /* Add on head */
            a->next = Modes.aircrafts;
            Modes.aircrafts = a;
        }
    }

    a->seen = time(NULL);
    a->messages++;

    if (mm->msgtype == 0 || mm->msgtype == 4 || mm->msgtype == 20) {
        a->altitude = mm->altitude;
    } else if (mm->msgtype == 17 || mm->msgtype == 18) {
        if (mm->metype >= 1 && mm->metype <= 4) {
            memcpy(a->flight, mm->flight, sizeof(a->flight));
        } else if (mm->metype >= 9 && mm->metype <= 18) {
            a->altitude = mm->altitude;
            if (mm->fflag) {
                a->odd_cprlat = mm->raw_latitude;
                a->odd_cprlon = mm->raw_longitude;
                a->odd_cprtime = mstime();
            } else {
                a->even_cprlat = mm->raw_latitude;
                a->even_cprlon = mm->raw_longitude;
                a->even_cprtime = mstime();
            }
            /* If the two data is less than 10 seconds apart, compute
             * the position. */
            if (llabs(a->even_cprtime - a->odd_cprtime) <= 10000) {
                double prev_lat = a->lat;
                double prev_lon = a->lon;
                decodeCPR(a);
                /* If we successfully decoded a new position, update the
                 * receiver reference position. This is used to decode
                 * surface positions which require a nearby reference. */
                if (a->lat != prev_lat || a->lon != prev_lon) {
                    if (Modes.ref_count == 0) {
                        Modes.ref_lat = a->lat;
                        Modes.ref_lon = a->lon;
                    } else {
                        /* Incremental average update. */
                        Modes.ref_lat += (a->lat - Modes.ref_lat) /
                                         (Modes.ref_count + 1);
                        Modes.ref_lon += (a->lon - Modes.ref_lon) /
                                         (Modes.ref_count + 1);
                    }
                    /* Cap at 10000 so the average can adapt if antenna moves. */
                    if (Modes.ref_count < 10000) Modes.ref_count++;
                }
            }
        } else if (mm->metype >= 5 && mm->metype <= 8) {
            /* Surface position message. Only decode if we have a reference
             * position from earlier airborne position decodes. Without a
             * reference, surface CPR decoding is ambiguous. */
            if (Modes.ref_count) {
                if (mm->ground_track_valid) a->track = mm->ground_track;
                if (mm->movement_valid)
                    a->speed = decodeMovementField(mm->movement);
                a->altitude = 0;  /* On ground. */
                decodeCPRSurface(a, mm->fflag, mm->raw_latitude,
                                 mm->raw_longitude);
            }
        } else if (mm->metype == 19) {
            if (mm->mesub == 1 || mm->mesub == 2) {
                a->speed = mm->velocity;
                a->track = mm->heading;
            }
        }
    }
    return a;
}

/* When in interactive mode If we don't receive new nessages within
 * MODES_INTERACTIVE_TTL seconds we remove the aircraft from the list. */
void interactiveRemoveStaleAircrafts(void) {
    struct aircraft *a = Modes.aircrafts;
    struct aircraft *prev = NULL;
    time_t now = time(NULL);

    while(a) {
        if ((now - a->seen) > Modes.interactive_ttl) {
            struct aircraft *next = a->next;
            /* Remove the element from the linked list, with care
             * if we are removing the first element. */
            free(a);
            if (!prev)
                Modes.aircrafts = next;
            else
                prev->next = next;
            a = next;
        } else {
            prev = a;
            a = a->next;
        }
    }
}

