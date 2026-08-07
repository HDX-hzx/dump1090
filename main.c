#include "dump1090.h"

/* =============================== Initialization =========================== */
void modesInitConfig(void) {
    Modes.gain = MODES_MAX_GAIN;
    Modes.dev_index = 0;
    Modes.enable_agc = 0;
    Modes.freq = MODES_DEFAULT_FREQ;
    Modes.filename = NULL;
    Modes.fix_errors = 1;
    Modes.check_crc = 1;
    Modes.raw = 0;
    Modes.net = 0;
    Modes.net_only = 0;
    Modes.onlyaddr = 0;
    Modes.debug = 0;
    Modes.interactive = 0;
    Modes.interactive_rows = MODES_INTERACTIVE_ROWS;
    Modes.interactive_ttl = MODES_INTERACTIVE_TTL;
    Modes.aggressive = 0;
    Modes.interactive_rows = getTermRows();
    Modes.loop = 0;
    Modes.ppm_error = 0;
}

void modesInit(void) {
    int i, q;

    pthread_mutex_init(&Modes.data_mutex,NULL);
    pthread_cond_init(&Modes.data_cond,NULL);
    /* We add a full message minus a final bit to the length, so that we
     * can carry the remaining part of the buffer that we can't process
     * in the message detection loop, back at the start of the next data
     * to process. This way we are able to also detect messages crossing
     * two reads. */
    Modes.data_len = MODES_DATA_LEN + (MODES_FULL_LEN-1)*4;
    Modes.data_ready = 0;
    /* Allocate the ICAO address cache. We use two uint32_t for every
     * entry because it's a addr / timestamp pair for every entry. */
    Modes.icao_cache = malloc(sizeof(uint32_t)*MODES_ICAO_CACHE_LEN*2);
    if (Modes.icao_cache == NULL) {
        fprintf(stderr, "Out of memory allocating the ICAO cache.\n");
        exit(1);
    }
    memset(Modes.icao_cache,0,sizeof(uint32_t)*MODES_ICAO_CACHE_LEN*2);
    Modes.aircrafts = NULL;
    Modes.interactive_last_update = 0;
    if ((Modes.data = malloc(Modes.data_len)) == NULL ||
        (Modes.magnitude = malloc(Modes.data_len*2)) == NULL) {
        fprintf(stderr, "Out of memory allocating data buffer.\n");
        exit(1);
    }
    memset(Modes.data,127,Modes.data_len);

    /* Populate the I/Q -> Magnitude lookup table. It is used because
     * sqrt or round may be expensive and performance may vary a lot
     * depending on the libc used.
     *
     * Note that we don't need to fill the table for negative values, as
     * we square both i and q to take the magnitude. So the maximum absolute
     * value of i and q is 128, thus the maximum magnitude we get is:
     *
     * sqrt(128*128+128*128) = ~181.02
     *
     * Then, to retain the full resolution and be able to distinguish among
     * every pair of I/Q values, we scale this range from the float range
     * 0-181 to the uint16_t range of 0-65536 by multiplying for 360. */
    Modes.maglut = malloc(129*129*2);
    if (Modes.maglut == NULL) {
        fprintf(stderr, "Out of memory allocating the magnitude lookup table.\n");
        exit(1);
    }
    for (i = 0; i <= 128; i++) {
        for (q = 0; q <= 128; q++) {
            Modes.maglut[i*129+q] = round(sqrt(i*i+q*q)*360);
        }
    }

    /* Statistics */
    Modes.stat_valid_preamble = 0;
    Modes.stat_demodulated = 0;
    Modes.stat_goodcrc = 0;
    Modes.stat_badcrc = 0;
    Modes.stat_fixed = 0;
    Modes.stat_single_bit_fix = 0;
    Modes.stat_two_bits_fix = 0;
    Modes.stat_http_requests = 0;
    Modes.stat_sbs_connections = 0;
    Modes.stat_out_of_phase = 0;
    Modes.exit = 0;

    /* Initialize error correction syndrome table. */
    modesInitErrorInfo();
}

/* ============================== Snip mode ================================= */

/* Get raw IQ samples and filter everything is < than the specified level
 * for more than 256 samples in order to reduce example file size. */
void snipMode(int level) {
    int i, q;
    long long c = 0;

    while ((i = getchar()) != EOF && (q = getchar()) != EOF) {
        if (abs(i-127) < level && abs(q-127) < level) {
            c++;
            if (c > MODES_PREAMBLE_US*4) continue;
        } else {
            c = 0;
        }
        putchar(i);
        putchar(q);
    }
}

/* ================================ Main ==================================== */
void showHelp(void) {
    printf(
"--device-index <index>   Select RTL device (default: 0).\n"
"--gain <db>              Set gain (default: max gain. Use -100 for auto-gain).\n"
"--enable-agc             Enable the Automatic Gain Control (default: off).\n"
"--freq <hz>              Set frequency (default: 1090 Mhz).\n"
"--ppm <error>            Set receiver error in parts per million (default: 0).\n"
"--ifile <filename>       Read data from file (use '-' for stdin).\n"
"--loop                   With --ifile, read the same file in a loop.\n"
"--interactive            Interactive mode refreshing data on screen.\n"
"--interactive-rows <num> Max number of rows in interactive mode (default: 15).\n"
"--interactive-ttl <sec>  Remove from list if idle for <sec> (default: 60).\n"
"--raw                    Show only messages hex values.\n"
"--net                    Enable networking.\n"
"--net-only               Enable just networking, no RTL device or file used.\n"
"--net-ro-port <port>     TCP listening port for raw output (default: 30002).\n"
"--net-ri-port <port>     TCP listening port for raw input (default: 30001).\n"
"--net-http-port <port>   HTTP server port (default: 8080).\n"
"--net-sbs-port <port>    TCP listening port for BaseStation format output (default: 30003).\n"
"--no-fix                 Disable single-bits error correction using CRC.\n"
"--no-crc-check           Disable messages with broken CRC (discouraged).\n"
"--aggressive             More CPU for more messages (two bits fixes, ...).\n"
"--stats                  With --ifile print stats at exit. No other output.\n"
"--onlyaddr               Show only ICAO addresses (testing purposes).\n"
"--metric                 Use metric units (meters, km/h, ...).\n"
"--snip <level>           Strip IQ file removing samples < level.\n"
"--debug <flags>          Debug mode (verbose), see README for details.\n"
"--help                   Show this help.\n"
"\n"
"Debug mode flags: d = Log frames decoded with errors\n"
"                  D = Log frames decoded with zero errors\n"
"                  c = Log frames with bad CRC\n"
"                  C = Log frames with good CRC\n"
"                  p = Log frames with bad preamble\n"
"                  n = Log network debugging info\n"
"                  j = Log frames to frames.js, loadable by debug.html.\n"
    );
}

/* This function is called a few times every second by main in order to
 * perform tasks we need to do continuously, like accepting new clients
 * from the net, refreshing the screen in interactive mode, and so forth. */
void backgroundTasks(void) {
    if (Modes.net) {
        modesAcceptClients();
        modesReadFromClients();
        interactiveRemoveStaleAircrafts();
    }

    /* Refresh screen when in interactive mode. */
    if (Modes.interactive &&
        (mstime() - Modes.interactive_last_update) >
        MODES_INTERACTIVE_REFRESH_TIME)
    {
        interactiveRemoveStaleAircrafts();
        interactiveShowData();
        Modes.interactive_last_update = mstime();
    }
}

int main(int argc, char **argv) {
    int j;

    /* Set sane defaults. */
    modesInitConfig();

    /* Parse the command line options */
    for (j = 1; j < argc; j++) {
        int more = j+1 < argc; /* There are more arguments. */

        if (!strcmp(argv[j],"--device-index") && more) {
            Modes.dev_index = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--gain") && more) {
            Modes.gain = atof(argv[++j])*10; /* Gain is in tens of DBs */
        } else if (!strcmp(argv[j],"--enable-agc")) {
            Modes.enable_agc++;
        } else if (!strcmp(argv[j],"--freq") && more) {
            Modes.freq = strtoll(argv[++j],NULL,10);
        } else if (!strcmp(argv[j],"--ppm") && more) {
            Modes.ppm_error = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--ifile") && more) {
            Modes.filename = strdup(argv[++j]);
        } else if (!strcmp(argv[j],"--loop")) {
            Modes.loop = 1;
        } else if (!strcmp(argv[j],"--no-fix")) {
            Modes.fix_errors = 0;
        } else if (!strcmp(argv[j],"--no-crc-check")) {
            Modes.check_crc = 0;
        } else if (!strcmp(argv[j],"--raw")) {
            Modes.raw = 1;
        } else if (!strcmp(argv[j],"--net")) {
            Modes.net = 1;
        } else if (!strcmp(argv[j],"--net-only")) {
            Modes.net = 1;
            Modes.net_only = 1;
        } else if (!strcmp(argv[j],"--net-ro-port") && more) {
            modesNetServices[MODES_NET_SERVICE_RAWO].port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-ri-port") && more) {
            modesNetServices[MODES_NET_SERVICE_RAWI].port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-http-port") && more) {
            modesNetServices[MODES_NET_SERVICE_HTTP].port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-sbs-port") && more) {
            modesNetServices[MODES_NET_SERVICE_SBS].port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--onlyaddr")) {
            Modes.onlyaddr = 1;
        } else if (!strcmp(argv[j],"--metric")) {
            Modes.metric = 1;
        } else if (!strcmp(argv[j],"--aggressive")) {
            Modes.aggressive++;
        } else if (!strcmp(argv[j],"--interactive")) {
            Modes.interactive = 1;
        } else if (!strcmp(argv[j],"--interactive-rows") && more) {
            Modes.interactive_rows = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--interactive-ttl") && more) {
            Modes.interactive_ttl = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--debug") && more) {
            char *f = argv[++j];
            while(*f) {
                switch(*f) {
                case 'D': Modes.debug |= MODES_DEBUG_DEMOD; break;
                case 'd': Modes.debug |= MODES_DEBUG_DEMODERR; break;
                case 'C': Modes.debug |= MODES_DEBUG_GOODCRC; break;
                case 'c': Modes.debug |= MODES_DEBUG_BADCRC; break;
                case 'p': Modes.debug |= MODES_DEBUG_NOPREAMBLE; break;
                case 'n': Modes.debug |= MODES_DEBUG_NET; break;
                case 'j': Modes.debug |= MODES_DEBUG_JS; break;
                default:
                    fprintf(stderr, "Unknown debugging flag: %c\n", *f);
                    exit(1);
                    break;
                }
                f++;
            }
        } else if (!strcmp(argv[j],"--stats")) {
            Modes.stats = 1;
        } else if (!strcmp(argv[j],"--snip") && more) {
            snipMode(atoi(argv[++j]));
            exit(0);
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else {
            fprintf(stderr,
                "Unknown or not enough arguments for option '%s'.\n\n",
                argv[j]);
            showHelp();
            exit(1);
        }
    }

    /* Setup for SIGWINCH for handling lines */
    if (Modes.interactive == 1) signal(SIGWINCH, sigWinchCallback);

    /* Initialization */
    modesInit();
    if (Modes.net_only) {
        fprintf(stderr,"Net-only mode, no RTL device or file open.\n");
    } else if (Modes.filename == NULL) {
        modesInitRTLSDR();
    } else {
        if (Modes.filename[0] == '-' && Modes.filename[1] == '\0') {
            Modes.fd = STDIN_FILENO;
        } else if ((Modes.fd = open(Modes.filename,O_RDONLY)) == -1) {
            perror("Opening data file");
            exit(1);
        }
    }
    if (Modes.net) modesInitNet();

    /* If the user specifies --net-only, just run in order to serve network
     * clients without reading data from the RTL device. */
    while (Modes.net_only) {
        backgroundTasks();
        modesWaitReadableClients(100);
    }

    /* Create the thread that will read the data from the device. */
    pthread_create(&Modes.reader_thread, NULL, readerThreadEntryPoint, NULL);

    pthread_mutex_lock(&Modes.data_mutex);
    while(1) {
        if (!Modes.data_ready) {
            pthread_cond_wait(&Modes.data_cond,&Modes.data_mutex);
            continue;
        }
        computeMagnitudeVector();

        /* Signal to the other thread that we processed the available data
         * and we want more (useful for --ifile). */
        Modes.data_ready = 0;
        pthread_cond_signal(&Modes.data_cond);

        /* Process data after releasing the lock, so that the capturing
         * thread can read data while we perform computationally expensive
         * stuff * at the same time. (This should only be useful with very
         * slow processors). */
        pthread_mutex_unlock(&Modes.data_mutex);
        detectModeS(Modes.magnitude, Modes.data_len/2);
        backgroundTasks();
        pthread_mutex_lock(&Modes.data_mutex);
        if (Modes.exit) break;
    }

    /* If --ifile and --stats were given, print statistics. */
    if (Modes.stats && Modes.filename) {
        printf("%lld valid preambles\n", Modes.stat_valid_preamble);
        printf("%lld demodulated again after phase correction\n",
            Modes.stat_out_of_phase);
        printf("%lld demodulated with zero errors\n",
            Modes.stat_demodulated);
        printf("%lld with good crc\n", Modes.stat_goodcrc);
        printf("%lld with bad crc\n", Modes.stat_badcrc);
        printf("%lld errors corrected\n", Modes.stat_fixed);
        printf("%lld single bit errors\n", Modes.stat_single_bit_fix);
        printf("%lld two bits errors\n", Modes.stat_two_bits_fix);
        printf("%lld total usable messages\n",
            Modes.stat_goodcrc + Modes.stat_fixed);
    }

    rtlsdr_close(Modes.dev);
    return 0;
}

