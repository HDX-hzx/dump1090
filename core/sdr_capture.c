#include "dump1090.h"

/* =============================== RTLSDR handling ========================== */
void modesInitRTLSDR(void) {
    int j;
    int device_count;
    char vendor[256], product[256], serial[256];

    device_count = rtlsdr_get_device_count();
    if (!device_count) {
        fprintf(stderr, "No supported RTLSDR devices found.\n");
        exit(1);
    }

    fprintf(stderr, "Found %d device(s):\n", device_count);
    for (j = 0; j < device_count; j++) {
        rtlsdr_get_device_usb_strings(j, vendor, product, serial);
        fprintf(stderr, "%d: %s, %s, SN: %s %s\n", j, vendor, product, serial,
            (j == Modes.dev_index) ? "(currently selected)" : "");
    }

    if (rtlsdr_open(&Modes.dev, Modes.dev_index) < 0) {
        fprintf(stderr, "Error opening the RTLSDR device: %s\n",
            strerror(errno));
        exit(1);
    }

    /* Set gain, frequency, sample rate, and reset the device. */
    rtlsdr_set_tuner_gain_mode(Modes.dev,
        (Modes.gain == MODES_AUTO_GAIN) ? 0 : 1);
    if (Modes.gain != MODES_AUTO_GAIN) {
        if (Modes.gain == MODES_MAX_GAIN) {
            /* Find the maximum gain available. */
            int numgains;
            int gains[100];

            numgains = rtlsdr_get_tuner_gains(Modes.dev, gains);
            Modes.gain = gains[numgains-1];
            fprintf(stderr, "Max available gain is: %.2f\n", Modes.gain/10.0);
        }
        rtlsdr_set_tuner_gain(Modes.dev, Modes.gain);
        fprintf(stderr, "Setting gain to: %.2f\n", Modes.gain/10.0);
    } else {
        fprintf(stderr, "Using automatic gain control.\n");
    }
    rtlsdr_set_freq_correction(Modes.dev, Modes.ppm_error);
    if (Modes.enable_agc) rtlsdr_set_agc_mode(Modes.dev, 1);
    rtlsdr_set_center_freq(Modes.dev, Modes.freq);
    rtlsdr_set_sample_rate(Modes.dev, MODES_DEFAULT_RATE);
    rtlsdr_reset_buffer(Modes.dev);
    fprintf(stderr, "Gain reported by device: %.2f\n",
        rtlsdr_get_tuner_gain(Modes.dev)/10.0);
}

/* We use a thread reading data in background, while the main thread
 * handles decoding and visualization of data to the user.
 *
 * The reading thread calls the RTLSDR API to read data asynchronously, and
 * uses a callback to populate the data buffer.
 * A Mutex is used to avoid races with the decoding thread. */
void rtlsdrCallback(unsigned char *buf, uint32_t len, void *ctx) {
    MODES_NOTUSED(ctx);

    pthread_mutex_lock(&Modes.data_mutex);
    if (len > MODES_DATA_LEN) len = MODES_DATA_LEN;
    /* Move the last part of the previous buffer, that was not processed,
     * on the start of the new buffer. */
    memcpy(Modes.data, Modes.data+MODES_DATA_LEN, (MODES_FULL_LEN-1)*4);
    /* Read the new data. */
    memcpy(Modes.data+(MODES_FULL_LEN-1)*4, buf, len);
    Modes.data_ready = 1;
    /* Signal to the other thread that new data is ready */
    pthread_cond_signal(&Modes.data_cond);
    pthread_mutex_unlock(&Modes.data_mutex);
}

/* This is used when --ifile is specified in order to read data from file
 * instead of using an RTLSDR device. */
void readDataFromFile(void) {
    pthread_mutex_lock(&Modes.data_mutex);
    while(1) {
        ssize_t nread, toread;
        unsigned char *p;

        if (Modes.data_ready) {
            pthread_cond_wait(&Modes.data_cond,&Modes.data_mutex);
            continue;
        }

        if (Modes.interactive) {
            /* When --ifile and --interactive are used together, slow down
             * playing at the natural rate of the RTLSDR received. */
            pthread_mutex_unlock(&Modes.data_mutex);
            usleep(5000);
            pthread_mutex_lock(&Modes.data_mutex);
        }

        /* Move the last part of the previous buffer, that was not processed,
         * on the start of the new buffer. */
        memcpy(Modes.data, Modes.data+MODES_DATA_LEN, (MODES_FULL_LEN-1)*4);
        toread = MODES_DATA_LEN;
        p = Modes.data+(MODES_FULL_LEN-1)*4;
        while(toread) {
            nread = read(Modes.fd, p, toread);
            /* In --file mode, seek the file again from the start
             * and re-play it if --loop was given. */
            if (nread == 0 &&
                Modes.filename != NULL &&
                Modes.fd != STDIN_FILENO &&
                Modes.loop)
            {
                if (lseek(Modes.fd,0,SEEK_SET) != -1) continue;
            }

            if (nread <= 0) {
                Modes.exit = 1; /* Signal the other thread to exit. */
                break;
            }
            p += nread;
            toread -= nread;
        }
        if (toread) {
            /* Not enough data on file to fill the buffer? Pad with
             * no signal. */
            memset(p,127,toread);
        }
        Modes.data_ready = 1;
        /* Signal to the other thread that new data is ready */
        pthread_cond_signal(&Modes.data_cond);
    }
}

/* We read data using a thread, so the main thread only handles decoding
 * without caring about data acquisition. */
void *readerThreadEntryPoint(void *arg) {
    MODES_NOTUSED(arg);

    if (Modes.filename == NULL) {
        rtlsdr_read_async(Modes.dev, rtlsdrCallback, NULL,
                              MODES_ASYNC_BUF_NUMBER,
                              MODES_DATA_LEN);
    } else {
        readDataFromFile();
    }
    return NULL;
}

/* Turn I/Q samples pointed by Modes.data into the magnitude vector
 * pointed by Modes.magnitude. */
void computeMagnitudeVector(void) {
    uint16_t *m = Modes.magnitude;
    unsigned char *p = Modes.data;
    uint32_t j;

    /* Compute the magnitudo vector. It's just SQRT(I^2 + Q^2), but
     * we rescale to the 0-255 range to exploit the full resolution. */
    for (j = 0; j < Modes.data_len; j += 2) {
        int i = p[j]-127;
        int q = p[j+1]-127;

        if (i < 0) i = -i;
        if (q < 0) q = -q;
        m[j/2] = Modes.maglut[i*129+q];
    }
}

/* Scale a sample value by a fixed-point factor, clamping to avoid overflow.
 * The scale is in 16384ths (so 16384 = 1.0, 32768 = 2.0, etc). */
uint16_t scaleSample(uint16_t v, uint16_t scale) {
    uint32_t result = (uint32_t)v * scale / 16384;
    return (result > 65535) ? 65535 : (uint16_t)result;
}

/* Phase enhancement algorithm.
 *
 * This function estimates whether we are sampling early or late relative to
 * the signal phase, and by how much, by examining the energy distribution
 * in the preamble. It then corrects each sample based on the decoded bit
 * value and the estimated phase offset.
 *
 * The preamble has known 1-bits at positions 0, 2, 7, 9 and known 0-bits
 * elsewhere. By looking at the energy that "leaks" into adjacent 0-bit
 * positions, we can estimate the phase offset:
 * - Energy at positions -1 and 6 indicates early sampling (we're catching
 *   some of the previous bit's energy)
 * - Energy at positions 3 and 10 indicates late sampling (we're catching
 *   some of the next bit's energy)
 *
 * Once we know the phase direction and magnitude, we walk through the
 * message and adjust each sample based on the bit value we decoded for
 * the adjacent bit.
 *
 * Original algorithm by Oliver Jowett. */
void applyPhaseCorrection(uint16_t *m) {
    int j;

    /* Measure energy in known 1-bit positions (should be high). */
    uint32_t onTime = m[0] + m[2] + m[7] + m[9];

    /* Measure energy leaking into positions before expected 1-bits.
     * This indicates we're sampling early (catching previous bit). */
    uint32_t early = (m[-1] + m[6]) * 2;

    /* Measure energy leaking into positions after expected 1-bits.
     * This indicates we're sampling late (catching next bit). */
    uint32_t late = (m[3] + m[10]) * 2;

    if (early > late) {
        /* Sampling late: each sample contains energy from the following bit.
         * Scale factor is proportional to how much we're off. */
        uint16_t scaleUp = 16384 + 16384 * early / (early + onTime);
        uint16_t scaleDown = 16384 - 16384 * early / (early + onTime);

        /* Last data sample: trailing bits are 0, so it will be low. Scale up. */
        m[MODES_PREAMBLE_US*2 + MODES_LONG_MSG_BITS*2 - 1] =
            scaleSample(m[MODES_PREAMBLE_US*2 + MODES_LONG_MSG_BITS*2 - 1], scaleUp);

        /* Walk backwards through message samples. */
        for (j = MODES_PREAMBLE_US*2 + MODES_LONG_MSG_BITS*2 - 2;
             j > MODES_PREAMBLE_US*2; j -= 2) {
            if (m[j] > m[j+1]) {
                /* This bit is 1: previous sample overlapped with high energy,
                 * so it's slightly high. Scale it down. */
                m[j-1] = scaleSample(m[j-1], scaleDown);
            } else {
                /* This bit is 0: previous sample overlapped with low energy,
                 * so it's slightly low. Scale it up. */
                m[j-1] = scaleSample(m[j-1], scaleUp);
            }
        }
    } else {
        /* Sampling early: each sample contains energy from the previous bit.
         * Scale factor is proportional to how much we're off. */
        uint16_t scaleUp = 16384 + 16384 * late / (late + onTime);
        uint16_t scaleDown = 16384 - 16384 * late / (late + onTime);

        /* First data sample: leading bits are 0, so it will be low. Scale up. */
        m[MODES_PREAMBLE_US*2] = scaleSample(m[MODES_PREAMBLE_US*2], scaleUp);

        /* Walk forwards through message samples. */
        for (j = MODES_PREAMBLE_US*2;
             j < MODES_PREAMBLE_US*2 + MODES_LONG_MSG_BITS*2 - 2; j += 2) {
            if (m[j] > m[j+1]) {
                /* This bit is 1: next sample overlapped with low energy
                 * (the 0 half of this bit), so it's slightly low. Scale up. */
                m[j+2] = scaleSample(m[j+2], scaleUp);
            } else {
                /* This bit is 0: next sample overlapped with high energy
                 * (the 1 half of this bit), so it's slightly high. Scale down. */
                m[j+2] = scaleSample(m[j+2], scaleDown);
            }
        }
    }
}

