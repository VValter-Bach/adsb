#include "sdr.h"
#include "data.h"

extern struct Modes Modes;

/* =============================== RTLSDR handling ========================== */

void modesInitRTLSDR(void)
{
    if (!rtlsdr_get_device_count()) {
        fprintf(stderr, "No supported RTLSDR devices found.\n");
        exit(1);
    }

    if (rtlsdr_open(&Modes.dev, 0) < 0) {
        fprintf(stderr, "Error opening the RTLSDR device: %s\n",
            strerror(errno));
        exit(1);
    }

    /* Set gain, frequency, sample rate, and reset the device. */
    rtlsdr_set_tuner_gain_mode(Modes.dev, 1);
    int numgains;
    int gains[100];
    numgains = rtlsdr_get_tuner_gains(Modes.dev, gains);
    rtlsdr_set_tuner_gain(Modes.dev, gains[numgains - 1]);
    rtlsdr_set_freq_correction(Modes.dev, 0);
    rtlsdr_set_center_freq(Modes.dev, MODES_DEFAULT_FREQ);
    rtlsdr_set_sample_rate(Modes.dev, MODES_DEFAULT_RATE);
    rtlsdr_reset_buffer(Modes.dev);
    fprintf(stderr, "Gain reported by device: %.2f\n",
        rtlsdr_get_tuner_gain(Modes.dev) / 10.0);
}

/* We use a thread reading data in background, while the main thread
 * handles decoding and visualization of data to the user.
 *
 * The reading thread calls the RTLSDR API to read data asynchronously, and
 * uses a callback to populate the data buffer.
 * A Mutex is used to avoid races with the decoding thread. */
void rtlsdrCallback(unsigned char* buf, uint32_t len, void* ctx)
{
    MODES_NOTUSED(ctx);

    pthread_mutex_lock(&Modes.data_mutex);
    if (len > MODES_DATA_LEN)
        len = MODES_DATA_LEN;
    /* Move the last part of the previous buffer, that was not processed,
     * on the start of the new buffer. */
    memcpy(Modes.data, Modes.data + MODES_DATA_LEN, (MODES_FULL_LEN - 1) * 4);
    /* Read the new data. */
    memcpy(Modes.data + (MODES_FULL_LEN - 1) * 4, buf, len);
    Modes.data_ready = 1;
    /* Signal to the other thread that new data is ready */
    pthread_cond_signal(&Modes.data_cond);
    pthread_mutex_unlock(&Modes.data_mutex);
}

/* We read data using a thread, so the main thread only handles decoding
 * without caring about data acquisition. */
void* readerThreadEntryPoint(void* arg)
{
    MODES_NOTUSED(arg);

    rtlsdr_read_async(Modes.dev, rtlsdrCallback, NULL,
        MODES_ASYNC_BUF_NUMBER,
        MODES_DATA_LEN);
    return NULL;
}
