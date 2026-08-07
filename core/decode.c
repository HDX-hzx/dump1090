#include "dump1090.h"

/* Decode the 13 bit AC altitude field (in DF 20 and others).
 * Returns the altitude, and set 'unit' to either MODES_UNIT_METERS
 * or MDOES_UNIT_FEETS. */
int decodeAC13Field(unsigned char *msg, int *unit) {
    int m_bit = msg[3] & (1<<6);
    int q_bit = msg[3] & (1<<4);

    if (!m_bit) {
        *unit = MODES_UNIT_FEET;
        if (q_bit) {
            /* N is the 11 bit integer resulting from the removal of bit
             * Q and M */
            int n = ((msg[2]&31)<<6) |
                    ((msg[3]&0x80)>>2) |
                    ((msg[3]&0x20)>>1) |
                     (msg[3]&15);
            /* The final altitude is due to the resulting number multiplied
             * by 25, minus 1000. */
            return n*25-1000;
        } else {
            /* TODO: Implement altitude where Q=0 and M=0 */
        }
    } else {
        *unit = MODES_UNIT_METERS;
        /* TODO: Implement altitude when meter unit is selected. */
    }
    return 0;
}

/* Decode the 12 bit AC altitude field (in DF 17 and others).
 * Returns the altitude or 0 if it can't be decoded. */
int decodeAC12Field(unsigned char *msg, int *unit) {
    int q_bit = msg[5] & 1;

    if (q_bit) {
        /* N is the 11 bit integer resulting from the removal of bit
         * Q */
        *unit = MODES_UNIT_FEET;
        int n = ((msg[5]>>1)<<4) | ((msg[6]&0xF0) >> 4);
        /* The final altitude is due to the resulting number multiplied
         * by 25, minus 1000. */
        return n*25-1000;
    } else {
        return 0;
    }
}

/* ca_str[] and fs_str[] moved to ui/interactive_display.c: they are pure
 * display strings (used only by displayModesMessage()) and don't belong
 * in the decoding layer. Declared `extern` in dump1090.h. */

/* ME message type to description table. */
char *me_str[] = {
};
char *getMEDescription(int metype, int mesub) {
    char *mename = "Unknown";

    if (metype >= 1 && metype <= 4)
        mename = "Aircraft Identification and Category";
    else if (metype >= 5 && metype <= 8)
        mename = "Surface Position";
    else if (metype >= 9 && metype <= 18)
        mename = "Airborne Position (Baro Altitude)";
    else if (metype == 19 && mesub >=1 && mesub <= 4)
        mename = "Airborne Velocity";
    else if (metype >= 20 && metype <= 22)
        mename = "Airborne Position (GNSS Height)";
    else if (metype == 23 && mesub == 0)
        mename = "Test Message";
    else if (metype == 24 && mesub == 1)
        mename = "Surface System Status";
    else if (metype == 28 && mesub == 1)
        mename = "Extended Squitter Aircraft Status (Emergency)";
    else if (metype == 28 && mesub == 2)
        mename = "Extended Squitter Aircraft Status (1090ES TCAS RA)";
    else if (metype == 29 && (mesub == 0 || mesub == 1))
        mename = "Target State and Status Message";
    else if (metype == 31 && (mesub == 0 || mesub == 1))
        mename = "Aircraft Operational Status Message";
    return mename;
}

/* Decode a raw Mode S message demodulated as a stream of bytes by
 * detectModeS(), and split it into fields populating a modesMessage
 * structure. */
void decodeModesMessage(struct modesMessage *mm, unsigned char *msg) {
    char *ais_charset = "?ABCDEFGHIJKLMNOPQRSTUVWXYZ????? ???????????????0123456789??????";

    /* Work on our local copy */
    memcpy(mm->msg,msg,MODES_LONG_MSG_BYTES);
    msg = mm->msg;

    /* Get the message type ASAP as other operations depend on this */
    mm->msgtype = msg[0]>>3;    /* Downlink Format */
    mm->msgbits = modesMessageLenByType(mm->msgtype);

    /* Compute the syndrome (CRC XOR transmitted CRC).
     * For a valid message, syndrome = 0. */
    mm->crc = modesChecksum(msg, mm->msgbits);

    /* Check CRC (syndrome == 0 means valid) and fix bit errors using
     * the CRC when possible (DF 11, 17, 18). Use fast table-based lookup. */
    mm->errorbit = -1;  /* No error */
    mm->iid = 0;        /* Interrogator Identifier (used by DF 11) */
    mm->crcok = (mm->crc == 0);

    if (!mm->crcok && Modes.fix_errors &&
        (mm->msgtype == 11 || mm->msgtype == 17 || mm->msgtype == 18))
    {
        int maxfix = Modes.aggressive ? MODES_MAX_BITERRORS : 1;
        int fixedbits[MODES_MAX_BITERRORS];
        int nfixed = fixBitErrors(msg, mm->msgbits, maxfix, fixedbits);
        if (nfixed > 0) {
            mm->crc = modesChecksum(msg, mm->msgbits);
            mm->crcok = (mm->crc == 0);
            mm->errorbit = fixedbits[0];
            if (nfixed == 1) {
                Modes.stat_single_bit_fix++;
            } else {
                Modes.stat_two_bits_fix++;
            }
        }
    }

    /* Note that most of the other computation happens *after* we fix
     * the single bit errors, otherwise we would need to recompute the
     * fields again. */
    mm->ca = msg[0] & 7;        /* Responder capabilities. */

    /* ICAO address */
    mm->aa1 = msg[1];
    mm->aa2 = msg[2];
    mm->aa3 = msg[3];

    /* DF 17 type (assuming this is a DF17, otherwise not used) */
    mm->metype = msg[4] >> 3;   /* Extended squitter message type. */
    mm->mesub = msg[4] & 7;     /* Extended squitter message subtype. */

    /* Fields for DF4,5,20,21 */
    mm->fs = msg[0] & 7;        /* Flight status for DF4,5,20,21 */
    mm->dr = msg[1] >> 3 & 31;  /* Request extraction of downlink request. */
    mm->um = ((msg[1] & 7)<<3)| /* Request extraction of downlink request. */
              msg[2]>>5;

    /* In the squawk (identity) field bits are interleaved like that
     * (message bit 20 to bit 32):
     *
     * C1-A1-C2-A2-C4-A4-ZERO-B1-D1-B2-D2-B4-D4
     *
     * So every group of three bits A, B, C, D represent an integer
     * from 0 to 7.
     *
     * The actual meaning is just 4 octal numbers, but we convert it
     * into a base ten number tha happens to represent the four
     * octal numbers.
     *
     * For more info: http://en.wikipedia.org/wiki/Gillham_code */
    {
        int a,b,c,d;

        a = ((msg[3] & 0x80) >> 5) |
            ((msg[2] & 0x02) >> 0) |
            ((msg[2] & 0x08) >> 3);
        b = ((msg[3] & 0x02) << 1) |
            ((msg[3] & 0x08) >> 2) |
            ((msg[3] & 0x20) >> 5);
        c = ((msg[2] & 0x01) << 2) |
            ((msg[2] & 0x04) >> 1) |
            ((msg[2] & 0x10) >> 4);
        d = ((msg[3] & 0x01) << 2) |
            ((msg[3] & 0x04) >> 1) |
            ((msg[3] & 0x10) >> 4);
        mm->identity = a*1000 + b*100 + c*10 + d;
    }

    /* DF 11, 17 & 18: try to populate our ICAO addresses whitelist.
     * DFs with an AP field (xored addr and crc), try to decode it. */
    if (mm->msgtype != 11 && mm->msgtype != 17 && mm->msgtype != 18) {
        /* Check if we can check the checksum for the Downlink Formats where
         * the checksum is xored with the aircraft ICAO address. We try to
         * brute force it using a list of recently seen aircraft addresses. */
        if (bruteForceAP(msg,mm)) {
            /* We recovered the message, mark the checksum as valid. */
            mm->crcok = 1;
        } else {
            mm->crcok = 0;
        }
    } else {
        /* If this is DF 11, 17 or 18 and the checksum was ok,
         * we can add this address to the list of recently seen
         * addresses. */
        uint32_t addr = (mm->aa1 << 16) | (mm->aa2 << 8) | mm->aa3;
        if (mm->crcok && mm->errorbit == -1) {
            addRecentlySeenICAOAddr(addr);
        }

        /* DF 11: For messages with small CRC residual (<80), treat as
         * valid IID (Interrogator Identifier) if ICAO is known. */
        if (mm->msgtype == 11 && !mm->crcok && mm->crc < 80) {
            if (ICAOAddressWasRecentlySeen(addr)) {
                mm->iid = mm->crc;
                mm->crcok = 1;
            }
        }
    }

    /* Decode 13 bit altitude for DF0, DF4, DF16, DF20 */
    if (mm->msgtype == 0 || mm->msgtype == 4 ||
        mm->msgtype == 16 || mm->msgtype == 20) {
        mm->altitude = decodeAC13Field(msg, &mm->unit);
    }

    /* Decode extended squitter specific stuff.
     * DF 17 = ADS-B from transponder, DF 18 = ADS-B from non-transponder.
     * Both use the same extended squitter format. */
    if (mm->msgtype == 17 || mm->msgtype == 18) {
        /* Decode the extended squitter message. */

        if (mm->metype >= 1 && mm->metype <= 4) {
            /* Aircraft Identification and Category */
            mm->aircraft_type = mm->metype-1;
            mm->flight[0] = ais_charset[msg[5]>>2];
            mm->flight[1] = ais_charset[((msg[5]&3)<<4)|(msg[6]>>4)];
            mm->flight[2] = ais_charset[((msg[6]&15)<<2)|(msg[7]>>6)];
            mm->flight[3] = ais_charset[msg[7]&63];
            mm->flight[4] = ais_charset[msg[8]>>2];
            mm->flight[5] = ais_charset[((msg[8]&3)<<4)|(msg[9]>>4)];
            mm->flight[6] = ais_charset[((msg[9]&15)<<2)|(msg[10]>>6)];
            mm->flight[7] = ais_charset[msg[10]&63];
            mm->flight[8] = '\0';
        } else if (mm->metype >= 5 && mm->metype <= 8) {
            /* Surface position Message.
             *
             * The message format is:
             * - Type code (TC): 5 bits (value 5-8)
             * - Movement: 7 bits (encoded ground speed)
             * - Ground track status: 1 bit (1 = valid)
             * - Ground track: 7 bits (0-127 -> 0-360 degrees)
             * - Time flag: 1 bit
             * - CPR format: 1 bit (0 = even, 1 = odd)
             * - CPR latitude: 17 bits
             * - CPR longitude: 17 bits */
            mm->movement = ((msg[4] & 0x07) << 4) | (msg[5] >> 4);
            mm->movement_valid = (mm->movement != 0);
            mm->ground_track_valid = (msg[5] >> 3) & 1;
            mm->ground_track = ((msg[5] & 0x07) << 4) | (msg[6] >> 4);
            mm->ground_track = mm->ground_track * 360 / 128;
            mm->fflag = (msg[6] >> 2) & 1;
            mm->tflag = (msg[6] >> 3) & 1;
            mm->raw_latitude = ((msg[6] & 3) << 15) |
                                (msg[7] << 7) |
                                (msg[8] >> 1);
            mm->raw_longitude = ((msg[8] & 1) << 16) |
                                 (msg[9] << 8) |
                                 msg[10];
        } else if (mm->metype >= 9 && mm->metype <= 18) {
            /* Airborne position Message */
            mm->fflag = msg[6] & (1<<2);
            mm->tflag = msg[6] & (1<<3);
            mm->altitude = decodeAC12Field(msg,&mm->unit);
            mm->raw_latitude = ((msg[6] & 3) << 15) |
                                (msg[7] << 7) |
                                (msg[8] >> 1);
            mm->raw_longitude = ((msg[8]&1) << 16) |
                                 (msg[9] << 8) |
                                 msg[10];
        } else if (mm->metype == 19 && mm->mesub >= 1 && mm->mesub <= 4) {
            /* Airborne Velocity Message */
            if (mm->mesub == 1 || mm->mesub == 2) {
                mm->ew_dir = (msg[5]&4) >> 2;
                mm->ew_velocity = ((msg[5]&3) << 8) | msg[6];
                mm->ns_dir = (msg[7]&0x80) >> 7;
                mm->ns_velocity = ((msg[7]&0x7f) << 3) | ((msg[8]&0xe0) >> 5);
                mm->vert_rate_source = (msg[8]&0x10) >> 4;
                mm->vert_rate_sign = (msg[8]&0x8) >> 3;
                mm->vert_rate = ((msg[8]&7) << 6) | ((msg[9]&0xfc) >> 2);
                /* Compute velocity and angle from the two speed
                 * components. */
                mm->velocity = sqrt(mm->ns_velocity*mm->ns_velocity+
                                    mm->ew_velocity*mm->ew_velocity);
                if (mm->velocity) {
                    int ewv = mm->ew_velocity;
                    int nsv = mm->ns_velocity;
                    double heading;

                    if (mm->ew_dir) ewv *= -1;
                    if (mm->ns_dir) nsv *= -1;
                    heading = atan2(ewv,nsv);

                    /* Convert to degrees. */
                    mm->heading = heading * 360 / (M_PI*2);
                    /* We don't want negative values but a 0-360 scale. */
                    if (mm->heading < 0) mm->heading += 360;
                } else {
                    mm->heading = 0;
                }
            } else if (mm->mesub == 3 || mm->mesub == 4) {
                mm->heading_is_valid = msg[5] & (1<<2);
                mm->heading = (360.0/128) * (((msg[5] & 3) << 5) |
                                              (msg[6] >> 3));
            }
        }
    }
    mm->phase_corrected = 0; /* Set to 1 by the caller if needed. */
}

/* Always positive MOD operation, used for CPR decoding. */
int cprModFunction(int a, int b) {
    int res = a % b;
    if (res < 0) res += b;
    return res;
}

/* The NL function uses the precomputed table from 1090-WP-9-14 */
int cprNLFunction(double lat) {
    if (lat < 0) lat = -lat; /* Table is simmetric about the equator. */
    if (lat < 10.47047130) return 59;
    if (lat < 14.82817437) return 58;
    if (lat < 18.18626357) return 57;
    if (lat < 21.02939493) return 56;
    if (lat < 23.54504487) return 55;
    if (lat < 25.82924707) return 54;
    if (lat < 27.93898710) return 53;
    if (lat < 29.91135686) return 52;
    if (lat < 31.77209708) return 51;
    if (lat < 33.53993436) return 50;
    if (lat < 35.22899598) return 49;
    if (lat < 36.85025108) return 48;
    if (lat < 38.41241892) return 47;
    if (lat < 39.92256684) return 46;
    if (lat < 41.38651832) return 45;
    if (lat < 42.80914012) return 44;
    if (lat < 44.19454951) return 43;
    if (lat < 45.54626723) return 42;
    if (lat < 46.86733252) return 41;
    if (lat < 48.16039128) return 40;
    if (lat < 49.42776439) return 39;
    if (lat < 50.67150166) return 38;
    if (lat < 51.89342469) return 37;
    if (lat < 53.09516153) return 36;
    if (lat < 54.27817472) return 35;
    if (lat < 55.44378444) return 34;
    if (lat < 56.59318756) return 33;
    if (lat < 57.72747354) return 32;
    if (lat < 58.84763776) return 31;
    if (lat < 59.95459277) return 30;
    if (lat < 61.04917774) return 29;
    if (lat < 62.13216659) return 28;
    if (lat < 63.20427479) return 27;
    if (lat < 64.26616523) return 26;
    if (lat < 65.31845310) return 25;
    if (lat < 66.36171008) return 24;
    if (lat < 67.39646774) return 23;
    if (lat < 68.42322022) return 22;
    if (lat < 69.44242631) return 21;
    if (lat < 70.45451075) return 20;
    if (lat < 71.45986473) return 19;
    if (lat < 72.45884545) return 18;
    if (lat < 73.45177442) return 17;
    if (lat < 74.43893416) return 16;
    if (lat < 75.42056257) return 15;
    if (lat < 76.39684391) return 14;
    if (lat < 77.36789461) return 13;
    if (lat < 78.33374083) return 12;
    if (lat < 79.29428225) return 11;
    if (lat < 80.24923213) return 10;
    if (lat < 81.19801349) return 9;
    if (lat < 82.13956981) return 8;
    if (lat < 83.07199445) return 7;
    if (lat < 83.99173563) return 6;
    if (lat < 84.89166191) return 5;
    if (lat < 85.75541621) return 4;
    if (lat < 86.53536998) return 3;
    if (lat < 87.00000000) return 2;
    else return 1;
}

int cprNFunction(double lat, int isodd) {
    int nl = cprNLFunction(lat) - isodd;
    if (nl < 1) nl = 1;
    return nl;
}

double cprDlonFunction(double lat, int isodd) {
    return 360.0 / cprNFunction(lat, isodd);
}

/* This algorithm comes from:
 * http://www.lll.lu/~edward/edward/adsb/DecodingADSBposition.html.
 *
 *
 * A few remarks:
 * 1) 131072 is 2^17 since CPR latitude and longitude are encoded in 17 bits.
 * 2) We assume that we always received the odd packet as last packet for
 *    simplicity. This may provide a position that is less fresh of a few
 *    seconds.
 */
void decodeCPR(struct aircraft *a) {
    const double AirDlat0 = 360.0 / 60;
    const double AirDlat1 = 360.0 / 59;
    double lat0 = a->even_cprlat;
    double lat1 = a->odd_cprlat;
    double lon0 = a->even_cprlon;
    double lon1 = a->odd_cprlon;

    /* Compute the Latitude Index "j" */
    int j = floor(((59*lat0 - 60*lat1) / 131072) + 0.5);
    double rlat0 = AirDlat0 * (cprModFunction(j,60) + lat0 / 131072);
    double rlat1 = AirDlat1 * (cprModFunction(j,59) + lat1 / 131072);

    if (rlat0 >= 270) rlat0 -= 360;
    if (rlat1 >= 270) rlat1 -= 360;

    /* Check that both are in the same latitude zone, or abort. */
    if (cprNLFunction(rlat0) != cprNLFunction(rlat1)) return;

    /* Compute ni and the longitude index m */
    if (a->even_cprtime > a->odd_cprtime) {
        /* Use even packet. */
        int ni = cprNFunction(rlat0,0);
        int m = floor((((lon0 * (cprNLFunction(rlat0)-1)) -
                        (lon1 * cprNLFunction(rlat0))) / 131072) + 0.5);
        a->lon = cprDlonFunction(rlat0,0) * (cprModFunction(m,ni)+lon0/131072);
        a->lat = rlat0;
    } else {
        /* Use odd packet. */
        int ni = cprNFunction(rlat1,1);
        int m = floor((((lon0 * (cprNLFunction(rlat1)-1)) -
                        (lon1 * cprNLFunction(rlat1))) / 131072.0) + 0.5);
        a->lon = cprDlonFunction(rlat1,1) * (cprModFunction(m,ni)+lon1/131072);
        a->lat = rlat1;
    }
    if (a->lon > 180) a->lon -= 360;
}

/* Decode surface CPR position using local decoding with reference position.
 *
 * Surface position messages use a different CPR encoding than airborne:
 * - Latitude zones cover 90 degrees instead of 360 degrees
 * - Only 17 bits are transmitted, but the encoding conceptually uses 19 bits,
 *   so there is inherent ambiguity requiring a reference position
 *
 * The reference position must be within 45 nautical miles of the true position
 * for unambiguous decoding. We use the average of all airborne positions we've
 * decoded as our reference, which works because if we can receive surface
 * traffic at an airport, we almost certainly receive airborne traffic too.
 *
 * This function uses local (relative) decoding with a single message, which
 * requires a reference position but doesn't need even/odd pairs. */
void decodeCPRSurface(struct aircraft *a, int fflag, int raw_lat, int raw_lon) {
    const double SurfDlat0 = 90.0 / 60;
    const double SurfDlat1 = 90.0 / 59;
    double dlat = fflag ? SurfDlat1 : SurfDlat0;
    double ref_lat = Modes.ref_lat;
    double ref_lon = Modes.ref_lon;
    double lat, lon;
    int j, ni, m;

    /* No reference position yet, can't decode surface position. */
    if (Modes.ref_count == 0) return;

    /* Compute latitude index j from reference latitude. */
    j = (int)floor(ref_lat / dlat) +
        (int)floor(0.5 + cprModFunction((int)ref_lat, (int)dlat) / dlat -
                   (double)raw_lat / 131072);

    lat = dlat * (j + (double)raw_lat / 131072);

    /* Pick the latitude solution closest to the reference. We may need to
     * try ±90 degrees since surface encoding covers only 90 degrees. */
    if (fabs(lat - ref_lat) > 45) {
        if (lat > ref_lat) lat -= 90;
        else lat += 90;
    }

    /* Sanity check: latitude must be in valid range. */
    if (lat < -90 || lat > 90) return;

    /* Compute longitude. */
    ni = cprNFunction(lat, fflag);
    if (ni == 0) ni = 1;  /* Avoid division by zero near poles. */
    m = (int)floor(ref_lon / (90.0 / ni)) +
        (int)floor(0.5 + cprModFunction((int)ref_lon, (int)(90.0 / ni)) /
                   (90.0 / ni) - (double)raw_lon / 131072);
    lon = (90.0 / ni) * (m + (double)raw_lon / 131072);

    /* Pick the longitude solution closest to reference. Surface encoding
     * covers only 90 degrees, so we may need to adjust by ±90 or ±180. */
    while (lon > ref_lon + 45) lon -= 90;
    while (lon < ref_lon - 45) lon += 90;

    /* Normalize longitude to -180..180 range. */
    if (lon > 180) lon -= 360;
    if (lon < -180) lon += 360;

    a->lat = lat;
    a->lon = lon;
}

/* Decode ground speed from surface movement field.
 * Returns speed in knots, or -1 if speed is not available. */
int decodeMovementField(int movement) {
    if (movement == 0) return -1;           /* Not available */
    if (movement == 1) return 0;            /* Stopped (< 0.125 kt) */
    if (movement <= 8) return (movement - 2) * 0.125 + 0.125;  /* 0.125-1 kt */
    if (movement <= 12) return (movement - 9) * 0.25 + 1;      /* 1-2 kt */
    if (movement <= 38) return (movement - 13) * 0.5 + 2;      /* 2-15 kt */
    if (movement <= 93) return (movement - 39) + 15;           /* 15-70 kt */
    if (movement <= 108) return (movement - 94) * 2 + 70;      /* 70-100 kt */
    if (movement <= 123) return (movement - 109) * 5 + 100;    /* 100-175 kt */
    return 175;  /* >= 175 kt */
}

