#include "dump1090.h"

/* ===================== Mode S detection and decoding  ===================== */

/* Parity table for MODE S Messages.
 * The table contains 112 elements, every element corresponds to a bit set
 * in the message, starting from the first bit of actual data after the
 * preamble.
 *
 * For messages of 112 bit, the whole table is used.
 * For messages of 56 bits only the last 56 elements are used.
 *
 * The algorithm is as simple as xoring all the elements in this table
 * for which the corresponding bit on the message is set to 1.
 *
 * The latest 24 elements in this table are set to 0 as the checksum at the
 * end of the message should not affect the computation.
 *
 * Note: this function can be used with DF11 and DF17, other modes have
 * the CRC xored with the sender address as they are reply to interrogations,
 * but a casual listener can't split the address from the checksum.
 */
uint32_t modes_checksum_table[112] = {
0x3935ea, 0x1c9af5, 0xf1b77e, 0x78dbbf, 0xc397db, 0x9e31e9, 0xb0e2f0, 0x587178,
0x2c38bc, 0x161c5e, 0x0b0e2f, 0xfa7d13, 0x82c48d, 0xbe9842, 0x5f4c21, 0xd05c14,
0x682e0a, 0x341705, 0xe5f186, 0x72f8c3, 0xc68665, 0x9cb936, 0x4e5c9b, 0xd8d449,
0x939020, 0x49c810, 0x24e408, 0x127204, 0x093902, 0x049c81, 0xfdb444, 0x7eda22,
0x3f6d11, 0xe04c8c, 0x702646, 0x381323, 0xe3f395, 0x8e03ce, 0x4701e7, 0xdc7af7,
0x91c77f, 0xb719bb, 0xa476d9, 0xadc168, 0x56e0b4, 0x2b705a, 0x15b82d, 0xf52612,
0x7a9309, 0xc2b380, 0x6159c0, 0x30ace0, 0x185670, 0x0c2b38, 0x06159c, 0x030ace,
0x018567, 0xff38b7, 0x80665f, 0xbfc92b, 0xa01e91, 0xaff54c, 0x57faa6, 0x2bfd53,
0xea04ad, 0x8af852, 0x457c29, 0xdd4410, 0x6ea208, 0x375104, 0x1ba882, 0x0dd441,
0xf91024, 0x7c8812, 0x3e4409, 0xe0d800, 0x706c00, 0x383600, 0x1c1b00, 0x0e0d80,
0x0706c0, 0x038360, 0x01c1b0, 0x00e0d8, 0x00706c, 0x003836, 0x001c1b, 0xfff409,
0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000
};

/* Compute the CRC of a Mode S message (data portion only).
 * This returns just the CRC computed from the data bits, without XORing
 * with the transmitted CRC/AP field. Used by bruteForceAP. */
uint32_t modesComputeCRC(unsigned char *msg, int bits) {
    uint32_t crc = 0;
    int offset = (bits == 112) ? 0 : (112-56);
    int j;

    /* Compute CRC of data portion (exclude last 24 CRC bits). */
    for(j = 0; j < bits - 24; j++) {
        int byte = j/8;
        int bit = j%8;
        int bitmask = 1 << (7-bit);

        /* If bit is set, xor with corresponding table entry. */
        if (msg[byte] & bitmask)
            crc ^= modes_checksum_table[j+offset];
    }
    return crc & 0x00FFFFFF;
}

/* Compute the CRC syndrome of a Mode S message.
 *
 * This function computes the CRC of the data portion (excluding the last
 * 24 bits which are the transmitted CRC), then XORs with the transmitted
 * CRC. The result is the "syndrome":
 *
 *   - For a valid message: syndrome = 0
 *   - For a corrupted message: syndrome = XOR of table entries for flipped bits
 *
 * This property makes syndrome-based error correction possible: we can
 * precompute syndromes for all possible single/double bit errors and look
 * them up in a table. */
uint32_t modesChecksum(unsigned char *msg, int bits) {
    uint32_t crc = modesComputeCRC(msg, bits);
    uint32_t rem;

    /* XOR with the transmitted CRC (last 3 bytes) to get the syndrome. */
    rem = ((uint32_t)msg[(bits/8)-3] << 16) |
          ((uint32_t)msg[(bits/8)-2] << 8) |
           (uint32_t)msg[(bits/8)-1];
    return (crc ^ rem) & 0x00FFFFFF; /* 24 bit syndrome. */
}

/* Given the Downlink Format (DF) of the message, return the message length
 * in bits. */
int modesMessageLenByType(int type) {
    if (type == 16 || type == 17 ||
        type == 18 || type == 19 ||
        type == 20 || type == 21)
        return MODES_LONG_MSG_BITS;
    else
        return MODES_SHORT_MSG_BITS;
}

/* Comparison function for qsort/bsearch on errorinfo by syndrome. */
int cmpErrorInfo(const void *a, const void *b) {
    const struct errorinfo *ea = (const struct errorinfo *)a;
    const struct errorinfo *eb = (const struct errorinfo *)b;
    if (ea->syndrome < eb->syndrome) return -1;
    if (ea->syndrome > eb->syndrome) return 1;
    return 0;
}

/* ===================== Error correction via syndrome table ================
 *
 * What is a syndrome?
 * -------------------
 * In CRC-based error detection, a "syndrome" is the CRC computed on a
 * received message. For a valid message, the syndrome is zero (or matches
 * the expected CRC). When bits get corrupted during transmission, the
 * syndrome becomes non-zero.
 *
 * The key insight is that the syndrome depends ONLY on which bits were
 * flipped, not on the message content itself. If we flip bit X in any
 * Mode S message, we always get the same syndrome value S_x. This property
 * comes from the linearity of CRC (it's based on polynomial arithmetic
 * over GF(2) - basically XOR operations).
 *
 * For two-bit errors, flipping bits X and Y produces syndrome S_x XOR S_y.
 *
 * So we can precompute a lookup table:
 *   - For each single bit position X, compute S_x
 *   - For each pair of bit positions X,Y, compute S_x XOR S_y
 *
 * When we receive a message with a non-zero syndrome S, we search the table
 * for S. If found, we know exactly which bit(s) to flip to fix the message.
 *
 * This turns O(n) or O(n^2) brute force error correction into O(log n)
 * binary search, making it much faster for real-time decoding.
 * ========================================================================= */

/* Compute the table of all syndromes for 1-bit and 2-bit error vectors.
 * We don't include the first 5 bits (the DF type field) since corrupting
 * those would change the message type and length, making correction risky. */
void modesInitErrorInfo(void) {
    unsigned char msg[MODES_LONG_MSG_BYTES];
    int i, j, n;
    uint32_t crc;

    n = 0;
    memset(bitErrorTable, 0, sizeof(bitErrorTable));
    memset(msg, 0, MODES_LONG_MSG_BYTES);

    /* Add all possible single and double bit errors.
     * Don't include errors in first 5 bits (DF type). */
    for (i = 5; i < MODES_LONG_MSG_BITS; i++) {
        int bytepos0 = (i >> 3);
        int mask0 = 1 << (7 - (i & 7));
        msg[bytepos0] ^= mask0;  /* Create error at bit i. */
        crc = modesChecksum(msg, MODES_LONG_MSG_BITS);

        /* Single bit error case. */
        bitErrorTable[n].syndrome = crc;
        bitErrorTable[n].bits = 1;
        bitErrorTable[n].pos[0] = i;
        bitErrorTable[n].pos[1] = -1;
        n++;

        /* Two-bit error cases. */
        for (j = i + 1; j < MODES_LONG_MSG_BITS; j++) {
            int bytepos1 = (j >> 3);
            int mask1 = 1 << (7 - (j & 7));
            msg[bytepos1] ^= mask1;  /* Create error at bit j. */
            crc = modesChecksum(msg, MODES_LONG_MSG_BITS);

            if (n >= NERRORINFO) break;

            bitErrorTable[n].syndrome = crc;
            bitErrorTable[n].bits = 2;
            bitErrorTable[n].pos[0] = i;
            bitErrorTable[n].pos[1] = j;
            n++;

            msg[bytepos1] ^= mask1;  /* Revert error at bit j. */
        }
        msg[bytepos0] ^= mask0;  /* Revert error at bit i. */
    }

    /* Sort by syndrome for binary search. */
    qsort(bitErrorTable, NERRORINFO, sizeof(struct errorinfo), cmpErrorInfo);
}

/* Given a received message, compute its syndrome and look it up in the
 * precomputed table. If we find a match, we know the exact bit positions
 * that were corrupted during transmission, and we can flip them back.
 *
 * The 'maxfix' parameter limits how many bits we're willing to correct:
 * fixing 1 bit is quite safe, but 2-bit correction has a higher chance
 * of "fixing" a random message into a valid but wrong one (birthday paradox).
 *
 * Returns the number of bits corrected (0 if syndrome not found or would
 * require too many bit fixes). If 'fixedbits' is not NULL, stores the
 * corrected bit positions there. */
int fixBitErrors(unsigned char *msg, int bits, int maxfix, int *fixedbits) {
    struct errorinfo *pei;
    struct errorinfo ei;
    int bitpos, offset, i, res;

    memset(&ei, 0, sizeof(struct errorinfo));
    ei.syndrome = modesChecksum(msg, bits);

    pei = bsearch(&ei, bitErrorTable, NERRORINFO,
                  sizeof(struct errorinfo), cmpErrorInfo);
    if (pei == NULL) {
        return 0;  /* No matching syndrome found. */
    }

    /* Check if syndrome fixes more bits than allowed. */
    if (pei->bits > maxfix) {
        return 0;
    }

    /* Check that all bit positions lie inside the message length. */
    offset = MODES_LONG_MSG_BITS - bits;
    for (i = 0; i < pei->bits; i++) {
        bitpos = pei->pos[i] - offset;
        if ((bitpos < 0) || (bitpos >= bits)) {
            return 0;
        }
    }

    /* Fix the bits. */
    res = 0;
    for (i = 0; i < pei->bits; i++) {
        bitpos = pei->pos[i] - offset;
        msg[bitpos >> 3] ^= (1 << (7 - (bitpos & 7)));
        if (fixedbits) {
            fixedbits[res++] = bitpos;
        } else {
            res++;
        }
    }
    return res;
}

/* Hash the ICAO address to index our cache of MODES_ICAO_CACHE_LEN
 * elements, that is assumed to be a power of two. */
uint32_t ICAOCacheHashAddress(uint32_t a) {
    /* The following three rounds wil make sure that every bit affects
     * every output bit with ~ 50% of probability. */
    a = ((a >> 16) ^ a) * 0x45d9f3b;
    a = ((a >> 16) ^ a) * 0x45d9f3b;
    a = ((a >> 16) ^ a);
    return a & (MODES_ICAO_CACHE_LEN-1);
}

/* Add the specified entry to the cache of recently seen ICAO addresses.
 * Note that we also add a timestamp so that we can make sure that the
 * entry is only valid for MODES_ICAO_CACHE_TTL seconds. */
void addRecentlySeenICAOAddr(uint32_t addr) {
    uint32_t h = ICAOCacheHashAddress(addr);
    Modes.icao_cache[h*2] = addr;
    Modes.icao_cache[h*2+1] = (uint32_t) time(NULL);
}

/* Returns 1 if the specified ICAO address was seen in a DF format with
 * proper checksum (not xored with address) no more than * MODES_ICAO_CACHE_TTL
 * seconds ago. Otherwise returns 0. */
int ICAOAddressWasRecentlySeen(uint32_t addr) {
    uint32_t h = ICAOCacheHashAddress(addr);
    uint32_t a = Modes.icao_cache[h*2];
    uint32_t t = Modes.icao_cache[h*2+1];

    return a && a == addr && time(NULL)-t <= MODES_ICAO_CACHE_TTL;
}

/* If the message type has the checksum xored with the ICAO address, try to
 * brute force it using a list of recently seen ICAO addresses.
 *
 * Do this in a brute-force fashion by xoring the predicted CRC with
 * the address XOR checksum field in the message. This will recover the
 * address: if we found it in our cache, we can assume the message is ok.
 *
 * This function expects mm->msgtype and mm->msgbits to be correctly
 * populated by the caller.
 *
 * On success the correct ICAO address is stored in the modesMessage
 * structure in the aa3, aa2, and aa1 fiedls.
 *
 * If the function successfully recovers a message with a correct checksum
 * it returns 1. Otherwise 0 is returned. */
int bruteForceAP(unsigned char *msg, struct modesMessage *mm) {
    unsigned char aux[MODES_LONG_MSG_BYTES];
    int msgtype = mm->msgtype;
    int msgbits = mm->msgbits;

    if (msgtype == 0 ||         /* Short air surveillance */
        msgtype == 4 ||         /* Surveillance, altitude reply */
        msgtype == 5 ||         /* Surveillance, identity reply */
        msgtype == 16 ||        /* Long Air-Air survillance */
        msgtype == 20 ||        /* Comm-A, altitude request */
        msgtype == 21 ||        /* Comm-A, identity request */
        msgtype == 24)          /* Comm-C ELM */
    {
        uint32_t addr;
        uint32_t crc;
        int lastbyte = (msgbits/8)-1;

        /* Work on a copy. */
        memcpy(aux,msg,msgbits/8);

        /* Compute the CRC of the message and XOR it with the AP field
         * so that we recover the address, because:
         *
         * (ADDR xor CRC) xor CRC = ADDR.
         * We use modesComputeCRC (not modesChecksum) to get just the CRC. */
        crc = modesComputeCRC(aux,msgbits);
        aux[lastbyte] ^= crc & 0xff;
        aux[lastbyte-1] ^= (crc >> 8) & 0xff;
        aux[lastbyte-2] ^= (crc >> 16) & 0xff;
        
        /* If the obtained address exists in our cache we consider
         * the message valid. */
        addr = aux[lastbyte] | (aux[lastbyte-1] << 8) | (aux[lastbyte-2] << 16);
        if (ICAOAddressWasRecentlySeen(addr)) {
            mm->aa1 = aux[lastbyte-2];
            mm->aa2 = aux[lastbyte-1];
            mm->aa3 = aux[lastbyte];
            return 1;
        }
    }
    return 0;
}

/* Detect a Mode S messages inside the magnitude buffer pointed by 'm' and of
 * size 'mlen' bytes. Every detected Mode S message is convert it into a
 * stream of bits and passed to the function to display it. */
void detectModeS(uint16_t *m, uint32_t mlen) {
    unsigned char bits[MODES_LONG_MSG_BITS];
    unsigned char msg[MODES_LONG_MSG_BITS/2];
    uint16_t aux[MODES_LONG_MSG_BITS*2];
    uint32_t j;
    int use_correction = 0;

    /* The Mode S preamble is made of impulses of 0.5 microseconds at
     * the following time offsets:
     *
     * 0   - 0.5 usec: first impulse.
     * 1.0 - 1.5 usec: second impulse.
     * 3.5 - 4   usec: third impulse.
     * 4.5 - 5   usec: last impulse.
     * 
     * Since we are sampling at 2 Mhz every sample in our magnitude vector
     * is 0.5 usec, so the preamble will look like this, assuming there is
     * an impulse at offset 0 in the array:
     *
     * 0   -----------------
     * 1   -
     * 2   ------------------
     * 3   --
     * 4   -
     * 5   --
     * 6   -
     * 7   ------------------
     * 8   --
     * 9   -------------------
     */
    for (j = 0; j < mlen - MODES_FULL_LEN*2; j++) {
        int low, high, delta, i, errors;
        int good_message = 0;

        if (use_correction) goto good_preamble; /* We already checked it. */

        /* First check of relations between the first 10 samples
         * representing a valid preamble. We don't even investigate further
         * if this simple test is not passed. */
        if (!(m[j] > m[j+1] &&
            m[j+1] < m[j+2] &&
            m[j+2] > m[j+3] &&
            m[j+3] < m[j] &&
            m[j+4] < m[j] &&
            m[j+5] < m[j] &&
            m[j+6] < m[j] &&
            m[j+7] > m[j+8] &&
            m[j+8] < m[j+9] &&
            m[j+9] > m[j+6]))
        {
            if (Modes.debug & MODES_DEBUG_NOPREAMBLE &&
                m[j] > MODES_DEBUG_NOPREAMBLE_LEVEL)
                dumpRawMessage("Unexpected ratio among first 10 samples",
                    msg, m, j);
            continue;
        }

        /* The samples between the two spikes must be < than the 2/3 of
         * the average of the high spikes level. We don't test bits too near to
         * the high levels as signals can be out of phase so part of the
         * energy can be in the near samples. */
        high = (m[j]+m[j+2]+m[j+7]+m[j+9])/6;
        if (m[j+4] >= high ||
            m[j+5] >= high)
        {
            if (Modes.debug & MODES_DEBUG_NOPREAMBLE &&
                m[j] > MODES_DEBUG_NOPREAMBLE_LEVEL)
                dumpRawMessage(
                    "Too high level in samples between 3 and 6",
                    msg, m, j);
            continue;
        }

        /* Similarly samples in the range 11-14 must be low, as it is the
         * space between the preamble and real data. Again we don't test
         * bits too near to high levels, see above. */
        if (m[j+11] >= high ||
            m[j+12] >= high ||
            m[j+13] >= high ||
            m[j+14] >= high)
        {
            if (Modes.debug & MODES_DEBUG_NOPREAMBLE &&
                m[j] > MODES_DEBUG_NOPREAMBLE_LEVEL)
                dumpRawMessage(
                    "Too high level in samples between 10 and 15",
                    msg, m, j);
            continue;
        }
        Modes.stat_valid_preamble++;

good_preamble:
        /* If the previous attempt with this message failed, retry using
         * magnitude correction. */
        if (use_correction) {
            memcpy(aux,m+j+MODES_PREAMBLE_US*2,sizeof(aux));
            /* Apply phase correction unconditionally on retry (j > 0 ensures
             * we can access m[-1] for preamble energy measurement). */
            if (j) {
                applyPhaseCorrection(m+j);
                Modes.stat_out_of_phase++;
            }
        }

        /* Decode all the next 112 bits, regardless of the actual message
         * size. We'll check the actual message type later. */
        errors = 0;
        for (i = 0; i < MODES_LONG_MSG_BITS*2; i += 2) {
            low = m[j+i+MODES_PREAMBLE_US*2];
            high = m[j+i+MODES_PREAMBLE_US*2+1];
            delta = low-high;
            if (delta < 0) delta = -delta;

            if (i > 0 && delta < 256) {
                bits[i/2] = bits[i/2-1];
            } else if (low == high) {
                /* Checking if two adiacent samples have the same magnitude
                 * is an effective way to detect if it's just random noise
                 * that was detected as a valid preamble. */
                bits[i/2] = 2; /* error */
                if (i < MODES_SHORT_MSG_BITS*2) errors++;
            } else if (low > high) {
                bits[i/2] = 1;
            } else {
                /* (low < high) for exclusion  */
                bits[i/2] = 0;
            }
        }

        /* Restore the original message if we used magnitude correction. */
        if (use_correction)
            memcpy(m+j+MODES_PREAMBLE_US*2,aux,sizeof(aux));

        /* Pack bits into bytes */
        for (i = 0; i < MODES_LONG_MSG_BITS; i += 8) {
            msg[i/8] =
                bits[i]<<7 | 
                bits[i+1]<<6 | 
                bits[i+2]<<5 | 
                bits[i+3]<<4 | 
                bits[i+4]<<3 | 
                bits[i+5]<<2 | 
                bits[i+6]<<1 | 
                bits[i+7];
        }

        int msgtype = msg[0]>>3;
        int msglen = modesMessageLenByType(msgtype)/8;

        /* Last check, high and low bits are different enough in magnitude
         * to mark this as real message and not just noise? */
        delta = 0;
        for (i = 0; i < msglen*8*2; i += 2) {
            delta += abs(m[j+i+MODES_PREAMBLE_US*2]-
                         m[j+i+MODES_PREAMBLE_US*2+1]);
        }
        delta /= msglen*4;

        /* Filter for an average delta of three is small enough to let almost
         * every kind of message to pass, but high enough to filter some
         * random noise. */
        if (delta < 10*255) {
            use_correction = 0;
            continue;
        }

        /* If we reached this point, and error is zero, we are very likely
         * with a Mode S message in our hands, but it may still be broken
         * and CRC may not be correct. This is handled by the next layer. */
        if (errors == 0 || (Modes.aggressive && errors < 3)) {
            struct modesMessage mm;

            /* Decode the received message and update statistics */
            decodeModesMessage(&mm,msg);

            /* Update statistics. */
            if (mm.crcok || use_correction) {
                if (errors == 0) Modes.stat_demodulated++;
                if (mm.errorbit == -1) {
                    if (mm.crcok)
                        Modes.stat_goodcrc++;
                    else
                        Modes.stat_badcrc++;
                } else {
                    Modes.stat_badcrc++;
                    Modes.stat_fixed++;
                    if (mm.errorbit < MODES_LONG_MSG_BITS)
                        Modes.stat_single_bit_fix++;
                    else
                        Modes.stat_two_bits_fix++;
                }
            }

            /* Output debug mode info if needed. */
            if (use_correction == 0) {
                if (Modes.debug & MODES_DEBUG_DEMOD)
                    dumpRawMessage("Demodulated with 0 errors", msg, m, j);
                else if (Modes.debug & MODES_DEBUG_BADCRC &&
                         mm.msgtype == 17 &&
                         (!mm.crcok || mm.errorbit != -1))
                    dumpRawMessage("Decoded with bad CRC", msg, m, j);
                else if (Modes.debug & MODES_DEBUG_GOODCRC && mm.crcok &&
                         mm.errorbit == -1)
                    dumpRawMessage("Decoded with good CRC", msg, m, j);
            }

            /* Skip this message if we are sure it's fine. */
            if (mm.crcok) {
                j += (MODES_PREAMBLE_US+(msglen*8))*2;
                good_message = 1;
                if (use_correction)
                    mm.phase_corrected = 1;
            }

            /* Pass data to the next layer */
            useModesMessage(&mm);
        } else {
            if (Modes.debug & MODES_DEBUG_DEMODERR && use_correction) {
                printf("The following message has %d demod errors\n", errors);
                dumpRawMessage("Demodulated with errors", msg, m, j);
            }
        }

        /* Retry with phase correction if possible. */
        if (!good_message && !use_correction) {
            j--;
            use_correction = 1;
        } else {
            use_correction = 0;
        }
    }
}

