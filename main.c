/* Mode1090, a Mode S messages decoder for RTLSDR devices.
 *
 * Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

#include "data.h"
#include "decode.h"
#include "gps.h"
#include "interactive.h"
#include "sdr.h"

struct Modes Modes;

/* =============================== Initialization =========================== */

void modesInitConfig(void)
{
    Modes.fix_errors = 1;
    Modes.check_crc = 1;
    Modes.interactive = 1;
    Modes.interactive_rows = MODES_INTERACTIVE_ROWS;
    Modes.interactive_ttl = MODES_INTERACTIVE_TTL;
    Modes.aggressive = 0;
    Modes.lat = 0.0;
    Modes.lon = 0.0;
}

void modesInit(void)
{
    int i, q;

    pthread_mutex_init(&Modes.data_mutex, NULL);
    pthread_cond_init(&Modes.data_cond, NULL);
    /* We add a full message minus a final bit to the length, so that we
     * can carry the remaining part of the buffer that we can't process
     * in the message detection loop, back at the start of the next data
     * to process. This way we are able to also detect messages crossing
     * two reads. */
    Modes.data_len = MODES_DATA_LEN + (MODES_FULL_LEN - 1) * 4;
    Modes.data_ready = 0;
    /* Allocate the ICAO address cache. We use two uint32_t for every
     * entry because it's a addr / timestamp pair for every entry. */
    Modes.icao_cache = malloc(sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2);
    memset(Modes.icao_cache, 0, sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2);
    Modes.aircrafts = NULL;
    Modes.interactive_last_update = 0;
    if ((Modes.data = malloc(Modes.data_len)) == NULL || (Modes.magnitude = malloc(Modes.data_len * 2)) == NULL) {
        fprintf(stderr, "Out of memory allocating data buffer.\n");
        exit(1);
    }
    memset(Modes.data, 127, Modes.data_len);

    /* Populate the I/Q -> Magnitude lookup table. It is used because
     * sqrt or round may be expensive and may vary a lot depending on
     * the libc used.
     *
     * We scale to 0-255 range multiplying by 1.4 in order to ensure that
     * every different I/Q pair will result in a different magnitude value,
     * not losing any resolution. */
    Modes.maglut = malloc(129 * 129 * 2);
    for (i = 0; i <= 128; i++) {
        for (q = 0; q <= 128; q++) {
            Modes.maglut[i * 129 + q] = round(sqrt(i * i + q * q) * 360);
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
}

/* ================================ Main ==================================== */

void showHelp(void)
{
    printf(
        "--lat <latitude>    Select the latitude of your position.\n"
        "--lon <longitude>   Select the longitude of your position.\n");
}

/* This function is called a few times every second by main in order to
 * perform tasks we need to do continuously, like accepting new clients
 * from the net, refreshing the screen in interactive mode, and so forth. */
void backgroundTasks(void)
{
    /* Refresh screen when in interactive mode. */
    if ((mstime() - Modes.interactive_last_update) > MODES_INTERACTIVE_REFRESH_TIME) {
        interactiveRemoveStaleAircrafts();
        interactiveShowData();
        Modes.interactive_last_update = mstime();
    }
}

int main(int argc, char** argv)
{
    int j;

    /* Set sane defaults. */
    modesInitConfig();

    /* Parse the command line options */
    for (j = 1; j < argc; j++) {
        int more = j+1 < argc; /* There are more arguments. */
        if (!strcmp(argv[j],"--lat") && more) {
            Modes.lat = atof(argv[++j]);
        }else if (!strcmp(argv[j],"--lon") && more) {
            Modes.lon = atof(argv[++j]);
        }else {
            fprintf(stderr,
                "Unknown or not enough arguments for option '%s'.\n\n",
                argv[j]);
            showHelp();
            exit(1);
        }
    }
    /* Initialization */
    modesInit();
    modesInitRTLSDR();

    /* Create the thread that will read the data from the device. */
    pthread_create(&Modes.reader_thread, NULL, readerThreadEntryPoint, NULL);

    pthread_mutex_lock(&Modes.data_mutex);
    while (1) {
        if (!Modes.data_ready) {
            pthread_cond_wait(&Modes.data_cond, &Modes.data_mutex);
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
        detectModeS(Modes.magnitude, Modes.data_len / 2);
        backgroundTasks();
        pthread_mutex_lock(&Modes.data_mutex);
        if (Modes.exit)
            break;
    }

    rtlsdr_close(Modes.dev);
    return 0;
}
