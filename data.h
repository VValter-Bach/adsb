#ifndef DATA_H
#define DATA_H

#include "rtl-sdr.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#define MODES_DEFAULT_RATE 2000000
#define MODES_DEFAULT_FREQ 1090000000
#define MODES_DEFAULT_WIDTH 1000
#define MODES_DEFAULT_HEIGHT 700
#define MODES_ASYNC_BUF_NUMBER 12
#define MODES_DATA_LEN (16 * 16384) /* 256k */
#define MODES_AUTO_GAIN -100 /* Use automatic gain. */
#define MODES_MAX_GAIN 999999 /* Use max available gain. */

#define MODES_PREAMBLE_US 8 /* microseconds */
#define MODES_LONG_MSG_BITS 112
#define MODES_SHORT_MSG_BITS 56
#define MODES_FULL_LEN (MODES_PREAMBLE_US + MODES_LONG_MSG_BITS)
#define MODES_LONG_MSG_BYTES (112 / 8)
#define MODES_SHORT_MSG_BYTES (56 / 8)

#define MODES_ICAO_CACHE_LEN 1024 /* Power of two required. */
#define MODES_ICAO_CACHE_TTL 60 /* Time to live of cached addresses. */
#define MODES_UNIT_FEET 0
#define MODES_UNIT_METERS 1

#define MODES_DEBUG_DEMOD (1 << 0)
#define MODES_DEBUG_DEMODERR (1 << 1)
#define MODES_DEBUG_BADCRC (1 << 2)
#define MODES_DEBUG_GOODCRC (1 << 3)
#define MODES_DEBUG_NOPREAMBLE (1 << 4)
#define MODES_DEBUG_NET (1 << 5)
#define MODES_DEBUG_JS (1 << 6)

/* When debug is set to MODES_DEBUG_NOPREAMBLE, the first sample must be
 * at least greater than a given level for us to dump the signal. */
#define MODES_DEBUG_NOPREAMBLE_LEVEL 25

#define MODES_INTERACTIVE_REFRESH_TIME 250 /* Milliseconds */
#define MODES_INTERACTIVE_ROWS 15 /* Rows on screen */
#define MODES_INTERACTIVE_TTL 60 /* TTL before being removed */

#define MODES_NET_MAX_FD 1024
#define MODES_NET_OUTPUT_SBS_PORT 30003
#define MODES_NET_OUTPUT_RAW_PORT 30002
#define MODES_NET_INPUT_RAW_PORT 30001
#define MODES_NET_HTTP_PORT 8080
#define MODES_CLIENT_BUF_SIZE 1024
#define MODES_NET_SNDBUF_SIZE (1024 * 64)

#define MODES_NOTUSED(V) ((void)V)

/* Structure used to describe an aircraft in iteractive mode. */
struct aircraft {
    uint32_t addr; /* ICAO address */
    char hexaddr[7]; /* Printable ICAO address */
    char flight[9]; /* Flight number */
    int altitude; /* Altitude */
    int speed; /* Velocity computed from EW and NS components. */
    int track; /* Angle of flight. */
    time_t seen; /* Time at which the last packet was received. */
    long messages; /* Number of Mode S messages received. */
    /* Encoded latitude and longitude as extracted by odd and even
     * CPR encoded messages. */
    int odd_cprlat;
    int odd_cprlon;
    int even_cprlat;
    int even_cprlon;
    double lat, lon; /* Coordinated obtained from CPR encoded data. */
    double distance; /* Distance to Location */
    long long odd_cprtime, even_cprtime;
    struct aircraft* next; /* Next aircraft in our linked list. */
};

/* Program global state. */
struct Modes{
    /* Internal state */
    pthread_t reader_thread;
    pthread_mutex_t data_mutex; /* Mutex to synchronize buffer access. */
    pthread_cond_t data_cond; /* Conditional variable associated. */
    unsigned char* data; /* Raw IQ samples buffer */
    uint16_t* magnitude; /* Magnitude vector */
    uint32_t data_len; /* Buffer length. */
    int data_ready; /* Data ready to be processed. */
    uint32_t* icao_cache; /* Recently seen ICAO addresses cache. */
    uint16_t* maglut; /* I/Q -> Magnitude lookup table. */
    int exit; /* Exit from the main loop when true. */

    /* RTLSDR */
    int dev_index;
    int gain;
    rtlsdr_dev_t* dev;
    int freq;

    /* Configuration */
    int fix_errors; /* Single bit error correction if true. */
    int check_crc; /* Only display messages with good CRC. */
    int debug; /* Debugging mode. */
    int interactive; /* Interactive mode */
    int interactive_rows; /* Interactive mode: max number of rows. */
    int interactive_ttl; /* Interactive mode: TTL before deletion. */
    int metric; /* Use metric units. */
    int aggressive; /* Aggressive detection algorithm. */

    /* Interactive mode */
    struct aircraft* aircrafts;
    long long interactive_last_update; /* Last screen update in milliseconds */
    double lat;
    double lon;

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
};

/* The struct we use to store information about a decoded message. */
struct modesMessage {
    /* Generic fields */
    unsigned char msg[MODES_LONG_MSG_BYTES]; /* Binary message. */
    int msgbits; /* Number of bits in message */
    int msgtype; /* Downlink format # */
    int crcok; /* True if CRC was valid */
    uint32_t crc; /* Message CRC */
    int errorbit; /* Bit corrected. -1 if no bit corrected. */
    int aa1, aa2, aa3; /* ICAO Address bytes 1 2 and 3 */
    int phase_corrected; /* True if phase correction was applied. */

    /* DF 11 */
    int ca; /* Responder capabilities. */

    /* DF 17 */
    int metype; /* Extended squitter message type. */
    int mesub; /* Extended squitter message subtype. */
    int heading_is_valid;
    int heading;
    int aircraft_type;
    int fflag; /* 1 = Odd, 0 = Even CPR message. */
    int tflag; /* UTC synchronized? */
    int raw_latitude; /* Non decoded latitude */
    int raw_longitude; /* Non decoded longitude */
    char flight[9]; /* 8 chars flight number. */
    int ew_dir; /* 0 = East, 1 = West. */
    int ew_velocity; /* E/W velocity. */
    int ns_dir; /* 0 = North, 1 = South. */
    int ns_velocity; /* N/S velocity. */
    int vert_rate_source; /* Vertical rate source. */
    int vert_rate_sign; /* Vertical rate sign. */
    int vert_rate; /* Vertical rate. */
    int velocity; /* Computed from EW and NS velocity. */

    /* DF4, DF5, DF20, DF21 */
    int fs; /* Flight status for DF4,5,20,21 */
    int dr; /* Request extraction of downlink request. */
    int um; /* Request extraction of downlink request. */
    int identity; /* 13 bits identity (Squawk). */

    /* Fields used by multiple message types. */
    int altitude, unit;
};

#endif //DATA_H