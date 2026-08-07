#ifndef DUMP1090_H
#define DUMP1090_H


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include "rtl-sdr.h"
#include "anet.h"

#define MODES_DEFAULT_RATE         2000000
#define MODES_DEFAULT_FREQ         1090000000
#define MODES_DEFAULT_WIDTH        1000
#define MODES_DEFAULT_HEIGHT       700
#define MODES_ASYNC_BUF_NUMBER     12
#define MODES_DATA_LEN             (16*16384)   /* 256k */
#define MODES_AUTO_GAIN            -100         /* Use automatic gain. */
#define MODES_MAX_GAIN             999999       /* Use max available gain. */

#define MODES_PREAMBLE_US 8       /* microseconds */
#define MODES_LONG_MSG_BITS 112
#define MODES_SHORT_MSG_BITS 56
#define MODES_FULL_LEN (MODES_PREAMBLE_US+MODES_LONG_MSG_BITS)
#define MODES_LONG_MSG_BYTES (112/8)
#define MODES_SHORT_MSG_BYTES (56/8)

#define MODES_ICAO_CACHE_LEN 1024 /* Power of two required. */
#define MODES_ICAO_CACHE_TTL 60   /* Time to live of cached addresses. */
#define MODES_UNIT_FEET 0
#define MODES_UNIT_METERS 1

#define MODES_MAX_BITERRORS 2     /* Maximum correctable bit errors. */
/* Number of entries in error syndrome table:
 * Single bit errors: 107 (bits 5-111)
 * Two-bit errors: 107*106/2 = 5671
 * Total: 5778 */
#define NERRORINFO 5778

#define MODES_DEBUG_DEMOD (1<<0)
#define MODES_DEBUG_DEMODERR (1<<1)
#define MODES_DEBUG_BADCRC (1<<2)
#define MODES_DEBUG_GOODCRC (1<<3)
#define MODES_DEBUG_NOPREAMBLE (1<<4)
#define MODES_DEBUG_NET (1<<5)
#define MODES_DEBUG_JS (1<<6)

/* When debug is set to MODES_DEBUG_NOPREAMBLE, the first sample must be
 * at least greater than a given level for us to dump the signal. */
#define MODES_DEBUG_NOPREAMBLE_LEVEL 25

#define MODES_INTERACTIVE_REFRESH_TIME 250      /* Milliseconds */
#define MODES_INTERACTIVE_ROWS 15               /* Rows on screen */
#define MODES_INTERACTIVE_TTL 60                /* TTL before being removed */

#define MODES_NET_MAX_FD 1024
#define MODES_NET_OUTPUT_SBS_PORT 30003
#define MODES_NET_OUTPUT_RAW_PORT 30002
#define MODES_NET_INPUT_RAW_PORT 30001
#define MODES_NET_HTTP_PORT 8080
#define MODES_CLIENT_BUF_SIZE 1024
#define MODES_NET_SNDBUF_SIZE (1024*64)

#define MODES_NOTUSED(V) ((void) V)

/* Structure used to describe a networking client. */
struct client {
    int fd;         /* File descriptor. */
    int service;    /* TCP port the client is connected to. */
    char buf[MODES_CLIENT_BUF_SIZE+1];    /* Read buffer. */
    int buflen;                         /* Amount of data on buffer. */
};

/* Structure used to describe an aircraft in iteractive mode. */
struct aircraft {
    uint32_t addr;      /* ICAO address */
    char hexaddr[7];    /* Printable ICAO address */
    char flight[9];     /* Flight number */
    int altitude;       /* Altitude */
    int speed;          /* Velocity computed from EW and NS components. */
    int track;          /* Angle of flight. */
    time_t seen;        /* Time at which the last packet was received. */
    long messages;      /* Number of Mode S messages received. */
    /* Encoded latitude and longitude as extracted by odd and even
     * CPR encoded messages. */
    int odd_cprlat;
    int odd_cprlon;
    int even_cprlat;
    int even_cprlon;
    double lat, lon;    /* Coordinated obtained from CPR encoded data. */
    long long odd_cprtime, even_cprtime;
    struct aircraft *next; /* Next aircraft in our linked list. */
};

/* Program global state. */
struct modes_state {
    /* Internal state */
    pthread_t reader_thread;
    pthread_mutex_t data_mutex;     /* Mutex to synchronize buffer access. */
    pthread_cond_t data_cond;       /* Conditional variable associated. */
    unsigned char *data;            /* Raw IQ samples buffer */
    uint16_t *magnitude;            /* Magnitude vector */
    uint32_t data_len;              /* Buffer length. */
    int fd;                         /* --ifile option file descriptor. */
    int data_ready;                 /* Data ready to be processed. */
    uint32_t *icao_cache;           /* Recently seen ICAO addresses cache. */
    uint16_t *maglut;               /* I/Q -> Magnitude lookup table. */
    int exit;                       /* Exit from the main loop when true. */

    /* RTLSDR */
    int dev_index;
    int gain;
    int enable_agc;
    rtlsdr_dev_t *dev;
    int freq;
    int ppm_error;

    /* Networking */
    char aneterr[ANET_ERR_LEN];
    struct client *clients[MODES_NET_MAX_FD]; /* Our clients. */
    int maxfd;                      /* Greatest fd currently active. */
    int sbsos;                      /* SBS output listening socket. */
    int ros;                        /* Raw output listening socket. */
    int ris;                        /* Raw input listening socket. */
    int https;                      /* HTTP listening socket. */

    /* Configuration */
    char *filename;                 /* Input form file, --ifile option. */
    int loop;                       /* Read input file again and again. */
    int fix_errors;                 /* Single bit error correction if true. */
    int check_crc;                  /* Only display messages with good CRC. */
    int raw;                        /* Raw output format. */
    int debug;                      /* Debugging mode. */
    int net;                        /* Enable networking. */
    int net_only;                   /* Enable just networking. */
    int interactive;                /* Interactive mode */
    int interactive_rows;           /* Interactive mode: max number of rows. */
    int interactive_ttl;            /* Interactive mode: TTL before deletion. */
    int stats;                      /* Print stats at exit in --ifile mode. */
    int onlyaddr;                   /* Print only ICAO addresses. */
    int metric;                     /* Use metric units. */
    int aggressive;                 /* Aggressive detection algorithm. */

    /* Interactive mode */
    struct aircraft *aircrafts;
    long long interactive_last_update;  /* Last screen update in milliseconds */

    /* Statistics */
    long long stat_valid_preamble;
    long long stat_demodulated;
    long long stat_goodcrc;
    long long stat_badcrc;
    long long stat_fixed;
    long long stat_single_bit_fix;
    long long stat_two_bits_fix;
    long long stat_http_requests;
    long long stat_sbs_connections;
    long long stat_out_of_phase;

    /* Reference location for surface position decoding.
     *
     * CPR (Compact Position Reporting) for surface positions works differently
     * than airborne positions: it uses a smaller coordinate range and requires
     * a reference position to resolve ambiguity. Instead of asking users to
     * configure their location, we automatically compute it by averaging all
     * successfully decoded airborne positions. Since surface decoding only
     * requires the reference to be within 45 nautical miles, even a rough
     * average of overflying aircraft positions works well. */
    double ref_lat, ref_lon;    /* Reference latitude and longitude. */
    int ref_count;              /* Number of positions used in the average. */
};

extern struct modes_state Modes;

/* The struct we use to store information about a decoded message. */
struct modesMessage {
    /* Generic fields */
    unsigned char msg[MODES_LONG_MSG_BYTES]; /* Binary message. */
    int msgbits;                /* Number of bits in message */
    int msgtype;                /* Downlink format # */
    int crcok;                  /* True if CRC was valid */
    uint32_t crc;               /* Message CRC */
    int errorbit;               /* Bit corrected. -1 if no bit corrected. */
    int aa1, aa2, aa3;          /* ICAO Address bytes 1 2 and 3 */
    int phase_corrected;        /* True if phase correction was applied. */

    /* DF 11 */
    int ca;                     /* Responder capabilities. */
    int iid;                    /* Interrogator Identifier (IID). */

    /* DF 17, 18 */
    int metype;                 /* Extended squitter message type. */
    int mesub;                  /* Extended squitter message subtype. */
    int heading_is_valid;
    int heading;
    int aircraft_type;
    int fflag;                  /* 1 = Odd, 0 = Even CPR message. */
    int tflag;                  /* UTC synchronized? */
    int raw_latitude;           /* Non decoded latitude */
    int raw_longitude;          /* Non decoded longitude */
    char flight[9];             /* 8 chars flight number. */
    int ew_dir;                 /* 0 = East, 1 = West. */
    int ew_velocity;            /* E/W velocity. */
    int ns_dir;                 /* 0 = North, 1 = South. */
    int ns_velocity;            /* N/S velocity. */
    int vert_rate_source;       /* Vertical rate source. */
    int vert_rate_sign;         /* Vertical rate sign. */
    int vert_rate;              /* Vertical rate. */
    int velocity;               /* Computed from EW and NS velocity. */

    /* DF 17, 18: Surface position (metype 5-8). */
    int movement;               /* Ground speed encoded value (0-124). */
    int movement_valid;         /* Movement field contains valid data. */
    int ground_track;           /* Ground track angle (0-360 degrees). */
    int ground_track_valid;     /* Ground track field contains valid data. */

    /* DF4, DF5, DF20, DF21 */
    int fs;                     /* Flight status for DF4,5,20,21 */
    int dr;                     /* Request extraction of downlink request. */
    int um;                     /* Request extraction of downlink request. */
    int identity;               /* 13 bits identity (Squawk). */

    /* Fields used by multiple message types. */
    int altitude, unit;
};

/* Structure for error syndrome lookup table entries. */
struct errorinfo {
    uint32_t syndrome;          /* CRC syndrome for this error pattern. */
    int bits;                   /* Number of bit errors (1 or 2). */
    int pos[MODES_MAX_BITERRORS]; /* Bit positions to correct. */
};

/* Error syndrome lookup table, sorted by syndrome for binary search. */
extern struct errorinfo bitErrorTable[NERRORINFO];

/* Display string tables, defined in ui/interactive_display.c. */
extern char *ca_str[8];
extern char *fs_str[8];

/* Network service table (raw in/out, HTTP, SBS): which port each service
 * listens on and which Modes.* socket field owns it. Defined in state.c
 * (it takes the address of fields inside Modes, so must be initialized
 * after Modes exists); indexed both from net/server.c and from main.c's
 * command line parsing (--net-ro-port and friends). */
#define MODES_NET_SERVICE_RAWO 0
#define MODES_NET_SERVICE_RAWI 1
#define MODES_NET_SERVICE_HTTP 2
#define MODES_NET_SERVICE_SBS 3
#define MODES_NET_SERVICES_NUM 4
struct net_service {
    char *descr;
    int *socket;
    int port;
};
extern struct net_service modesNetServices[MODES_NET_SERVICES_NUM];

/* ============================ Module APIs ================================ */
/* util.c */
long long mstime(void);

/* core/sdr_capture.c */
void modesInitRTLSDR(void);
void rtlsdrCallback(unsigned char *buf, uint32_t len, void *ctx);
void readDataFromFile(void);
void *readerThreadEntryPoint(void *arg);
void computeMagnitudeVector(void);
uint16_t scaleSample(uint16_t v, uint16_t scale);
void applyPhaseCorrection(uint16_t *m);

/* core/demod.c */
uint32_t modesComputeCRC(unsigned char *msg, int bits);
uint32_t modesChecksum(unsigned char *msg, int bits);
int modesMessageLenByType(int type);
int cmpErrorInfo(const void *a, const void *b);
void modesInitErrorInfo(void);
int fixBitErrors(unsigned char *msg, int bits, int maxfix, int *fixedbits);
uint32_t ICAOCacheHashAddress(uint32_t a);
void addRecentlySeenICAOAddr(uint32_t addr);
int ICAOAddressWasRecentlySeen(uint32_t addr);
int bruteForceAP(unsigned char *msg, struct modesMessage *mm);
void detectModeS(uint16_t *m, uint32_t mlen);

/* core/decode.c */
int decodeAC13Field(unsigned char *msg, int *unit);
int decodeAC12Field(unsigned char *msg, int *unit);
char *getMEDescription(int metype, int mesub);
void decodeModesMessage(struct modesMessage *mm, unsigned char *msg);
int cprModFunction(int a, int b);
int cprNLFunction(double lat);
int cprNFunction(double lat, int isodd);
double cprDlonFunction(double lat, int isodd);
void decodeCPR(struct aircraft *a);
void decodeCPRSurface(struct aircraft *a, int fflag, int raw_lat, int raw_lon);
int decodeMovementField(int movement);

/* core/aircraft_state.c */
void useModesMessage(struct modesMessage *mm);
struct aircraft *interactiveCreateAircraft(uint32_t addr);
struct aircraft *interactiveFindAircraft(uint32_t addr);
struct aircraft *interactiveReceiveData(struct modesMessage *mm);
void interactiveRemoveStaleAircrafts(void);

/* net/server.c */
void modesInitNet(void);
void modesAcceptClients(void);
void modesFreeClient(int fd);
void modesSendAllClients(int service, void *msg, int len);
void modesReadFromClient(struct client *c, char *sep, int(*handler)(struct client *));
void modesReadFromClients(void);
void modesWaitReadableClients(int timeout_ms);

/* net/protocols.c */
void modesSendRawOutput(struct modesMessage *mm);
void modesSendSBSOutput(struct modesMessage *mm, struct aircraft *a);
int hexDigitVal(int c);
int decodeHexMessage(struct client *c);

/* net/http_handler.c */
char *aircraftsToJson(int *len);
int handleHTTPRequest(struct client *c);

/* ui/interactive_display.c */
void dumpMagnitudeBar(int index, int magnitude);
void dumpMagnitudeVector(uint16_t *m, uint32_t offset);
void dumpRawMessageJS(char *descr, unsigned char *msg, uint16_t *m, uint32_t offset, int fixable);
void dumpRawMessage(char *descr, unsigned char *msg, uint16_t *m, uint32_t offset);
void displayModesMessage(struct modesMessage *mm);
void interactiveShowData(void);
void sigWinchCallback(int sig);
int getTermRows(void);

/* main.c */
void modesInitConfig(void);
void modesInit(void);
void snipMode(int level);
void showHelp(void);
void backgroundTasks(void);

#endif /* DUMP1090_H */
