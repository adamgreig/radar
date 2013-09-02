#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include "libbladeRF.h"

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"

char* output_filename = "./bladerf_samples.dat";

struct bladerf_config
{
    unsigned int tx_freq; // TX Frequency,   300MHz  to 3.8GHz, in Hz
    unsigned int rx_freq; // RX Frequency,   300MHz  to 3.8GHz, in Hz
    unsigned int tx_bw;   // TX Bandwidth,   1.5MHz  to  28MHz, in Hz
    unsigned int rx_bw;   // RX Bandwidth,   1.5MHz  to  28MHz, in Hz
    unsigned int tx_sr;   // TX Sample rate, 400kS/s to 40MS/s, in S/s
    unsigned int rx_sr;   // RX Sample rate, 400kS/s to 40MS/s, in S/s
    int txvga1; // Post-LPF gain, default -14dB, -35dB to -4dB, 1dB steps
    int txvga2; // PA gain,       default   0dB,   0dB to 25dB, 1dB steps
    int rxvga1; // Mixer gain,    default  30dB,   5dB to 30dB
    int rxvga2; // Post-LPF gain, default   3dB,   0dB to 60dB, 3dB steps
    int lna;    // BLADERF_LNA_GAIN_BYPASS/MID/MAX, default MAX
};

struct bladerf_stream_data
{
    void           **buffers;
    size_t         num_buffers;
    size_t         samples_per_buffer;
    unsigned int   next_buffer;
    bladerf_module module;
    FILE           *fout;
    int            samples_left;
};

struct bladerf_thread_data
{
    struct bladerf* dev;
    struct bladerf_stream* stream;
    struct bladerf_stream_data* stream_data;
    pthread_mutex_t lock;
    int rv;
};


int configure_bladerf(struct bladerf** dev, struct bladerf_config config)
{
    unsigned int abw, asr;
    int status;

    setlocale(LC_NUMERIC, "");

    printf("%-50s", "Connecting to device... ");
    fflush(stdout);
    status = bladerf_open(dev, NULL);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-50s", "Checking FPGA status... ");
    fflush(stdout);
    status = bladerf_is_fpga_configured(*dev);
    if(status < 0) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    } else if(status == 0) {
        printf(KRED "Failed: FPGA not loaded" KNRM "\n");
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz... ", "Tuning TX to:", config.tx_freq);
    fflush(stdout);
    status = bladerf_set_frequency(*dev, BLADERF_MODULE_TX, config.tx_freq);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz... ", "Tuning RX to:", config.rx_freq);
    fflush(stdout);
    status = bladerf_set_frequency(*dev, BLADERF_MODULE_RX, config.rx_freq);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz... ", "Setting TX bandwidth to:", config.tx_bw);
    fflush(stdout);
    status = bladerf_set_bandwidth(*dev, BLADERF_MODULE_TX,
                                   config.tx_bw, &abw);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz\n", "Actual bandwidth:", abw);
    if(abw != config.tx_bw) {
        printf("Actual bandwidth not equal to desired bandwidth, quitting.\n");
        return 1;
    }

    printf("%-30s %'13uHz... ", "Setting RX bandwidth to:", config.rx_bw);
    fflush(stdout);
    status = bladerf_set_bandwidth(*dev, BLADERF_MODULE_RX,
                                   config.rx_bw, &abw);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz\n", "Actual bandwidth:", abw);
    if(abw != config.rx_bw) {
        printf("Actual bandwidth not equal to desired bandwidth, quitting.\n");
        return 1;
    }

    printf("%-30s %'12usps... ", "Setting TX sampling rate to:", config.tx_sr);
    fflush(stdout);
    status = bladerf_set_sample_rate(*dev, BLADERF_MODULE_TX,
                                     config.tx_sr, &asr);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'12usps\n", "Actual sampling rate:", asr);
    if(asr != config.tx_sr) {
        printf("Actual sampling rate not equal to desired sampling rate, "
               "quitting.\n");
        return 1;
    }

    printf("%-30s %'12usps... ", "Setting RX sampling rate to:", config.rx_sr);
    fflush(stdout);
    status = bladerf_set_sample_rate(*dev, BLADERF_MODULE_RX,
                                     config.rx_sr, &asr);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'12usps\n", "Actual sampling rate:", asr);
    if(asr != config.rx_sr) {
        printf("Actual sampling rate not equal to desired sampling rate, "
               "quitting.\n");
        return 1;
    }

    printf("%-30s %+13ddB... ", "Setting TXVGA1 gain to:", config.txvga1);
    fflush(stdout);
    status = bladerf_set_txvga1(*dev, config.txvga1);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %+13ddB... ", "Setting TXVGA2 gain to:", config.txvga2);
    fflush(stdout);
    status = bladerf_set_txvga2(*dev, config.txvga2);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %+13ddB... ", "Setting RXVGA1 gain to:", config.rxvga1);
    fflush(stdout);
    status = bladerf_set_rxvga1(*dev, config.rxvga1);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %+13ddB... ", "Setting RXVGA2 gain to:", config.rxvga2);
    fflush(stdout);
    status = bladerf_set_rxvga2(*dev, config.rxvga2);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %15d... ", "Setting LNA gain to:", config.lna);
    fflush(stdout);
    status = bladerf_set_lna_gain(*dev, config.lna);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("All set up.\n");

    return 0;
}


int enable(struct bladerf** dev, bool enabled)
{
    int status;

    if(enabled)
        printf("%-50s", "Enabling TX... ");
    else
        printf("%-50s", "Disabling TX... ");
    fflush(stdout);
    status = bladerf_enable_module(*dev, BLADERF_MODULE_TX, enabled);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    if(enabled)
        printf("%-50s", "Enabling RX... ");
    else
        printf("%-50s", "Disabling RX... ");
    fflush(stdout);
    status = bladerf_enable_module(*dev, BLADERF_MODULE_RX, enabled);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    return 0;
}


void* stream_cb(struct bladerf *dev, struct bladerf_stream *stream,
                struct bladerf_metadata *md, void *samples, size_t n_samples,
                void *user_data)
{
    struct bladerf_stream_data *data = user_data; 
    size_t i;
    int16_t *sample = samples;

    if(data->module == BLADERF_MODULE_RX) {
        printf("r");
        fflush(stdout);
        for(i=0; i<n_samples; i++) {
            *sample &= 0x0fff;
            if(*sample & 0x0800)
                *sample |= 0xf000;
            *(sample+1) &= 0x0fff;
            if(*(sample+1) & 0x0800)
                *(sample+1) |= 0xf000;
            fwrite(sample, 2, 2, data->fout);
            sample += 2;
        }
    } else {
        printf("t");
        fflush(stdout);
        memset(samples, 0, n_samples * 2 * 2);
        for(i=0; i<n_samples/200 - 1; i++) {
            sample[i*2*200 + 0] = 2048;
            sample[i*2*200 + 2] = 2048;
            sample[i*2*200 + 4] = 2048;
            sample[i*2*200 + 6] = 2048;
            sample[i*2*200 + 8] = 2048;
        }
    }

    data->samples_left -= n_samples;
    if(data->samples_left <= 0) {
        return NULL;
    }

    void *rv = data->buffers[data->next_buffer];
    data->next_buffer = (data->next_buffer + 1) % data->num_buffers;
    return rv;
}


int setup_rx_stream(struct bladerf* dev, struct bladerf_stream* stream,
                    struct bladerf_stream_data* stream_data)
{
    int status;

    printf("%-10s %-39s", "Opening", output_filename);
    fflush(stdout);
    stream_data->fout = fopen(output_filename, "wb");
    if(!stream_data->fout) {
        printf(KRED "Failed: %s" KNRM "\n", strerror(errno));
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    stream_data->next_buffer = 0;
    stream_data->module = BLADERF_MODULE_RX;

    printf("%-50s", "Initialising RX data stream... ");
    fflush(stdout);
    status = bladerf_init_stream(&stream, dev, stream_cb,
                                 &stream_data->buffers,
                                 stream_data->num_buffers,
                                 BLADERF_FORMAT_SC16_Q12,
                                 stream_data->samples_per_buffer,
                                 stream_data->num_buffers, &stream_data);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");
    return 0;
}


int setup_tx_stream(struct bladerf* dev, struct bladerf_stream* stream,
                    struct bladerf_stream_data* stream_data)
{
    int status;

    stream_data->next_buffer = 0;
    stream_data->module = BLADERF_MODULE_TX;

    printf("%-50s", "Initialising TX data stream... ");
    fflush(stdout);
    status = bladerf_init_stream(&stream, dev, stream_cb,
                                 &stream_data->buffers,
                                 stream_data->num_buffers,
                                 BLADERF_FORMAT_SC16_Q12,
                                 stream_data->samples_per_buffer,
                                 stream_data->num_buffers, &stream_data);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");
    return 0;
}


void* txrx_thread(void* arg)
{
    struct bladerf_thread_data* thread_data = (struct bladerf_thread_data*)arg;

    thread_data->rv = setup_tx_stream(thread_data->dev, thread_data->stream,
                                      thread_data->stream_data);
    if(thread_data->rv) {
        return NULL;
    }

    pthread_mutex_lock(&(thread_data->lock));

    thread_data->rv = bladerf_stream(thread_data->dev,
                                     thread_data->stream_data->module,
                                     BLADERF_FORMAT_SC16_Q12,
                                     thread_data->stream);

    pthread_mutex_unlock(&(thread_data->lock));
    return NULL;
}


int main(int argc, char* argv) {
    int status;
    struct bladerf *dev;
    struct bladerf_config cfg;
    struct bladerf_stream* tx_stream;
    struct bladerf_stream* rx_stream;
    struct bladerf_stream_data tx_stream_data;
    struct bladerf_stream_data rx_stream_data;
    struct bladerf_thread_data tx_thread_data;
    struct bladerf_thread_data rx_thread_data;
    pthread_t tx_thread_pth;
    pthread_t rx_thread_pth;

    cfg.tx_freq = 3410000000;  // 3.41GHz TX puts us a bit into the band
    cfg.rx_freq = 3400000000;  // 3.40GHz RX puts the TX signals away from LO
    cfg.tx_bw = 28000000;      // Full bandwidth on TX for sharp pulses
    cfg.rx_bw = 28000000;      // Full bandwidth on RX for sharp returns
    cfg.tx_sr = 40000000;      // 10MS/s will do for TXing pulses
    cfg.rx_sr = 40000000;      // 40MS/s for RX for best possible resolution
    cfg.txvga1 = -14;          // Default
    cfg.txvga2 = 0;            // Default
    cfg.rxvga1 = 30;           // Default
    cfg.rxvga2 = 3;            // Default
    cfg.lna = BLADERF_LNA_GAIN_MAX;

    if(configure_bladerf(&dev, cfg)) {
        return 1;
    }

    tx_stream_data.num_buffers        = 2;
    tx_stream_data.samples_per_buffer = 1048576;   //    1MS
//  tx_stream_data.samples_left       = 2621440;   //  2.5MS
    tx_stream_data.samples_left       = 10485760;  //   10MS

    /*
    if(setup_tx_stream(dev, tx_stream, &tx_stream_data)) {
        return 1;
    }
    */

    rx_stream_data.num_buffers        = 2;
    rx_stream_data.samples_per_buffer = 1048576;   //    1MS
    rx_stream_data.samples_left       = 10485760;  //   10MS

    /*
    if(setup_rx_stream(dev, rx_stream, &rx_stream_data)) {
        return 1;
    }
    */

    tx_thread_data.dev = dev;
    tx_thread_data.stream = tx_stream;
    tx_thread_data.stream_data = &tx_stream_data;
    pthread_mutex_init(&(tx_thread_data.lock), NULL);
    pthread_mutex_lock(&(tx_thread_data.lock));

    printf("%-50s", "Creating TX thread... ");
    status = pthread_create(&tx_thread_pth, NULL, txrx_thread,
                            &tx_thread_data);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", strerror(status));
        enable(&dev, false);
        bladerf_deinit_stream(rx_stream);
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    rx_thread_data.dev = dev;
    rx_thread_data.stream = rx_stream;
    rx_thread_data.stream_data = &rx_stream_data;
    pthread_mutex_init(&(rx_thread_data.lock), NULL);
    pthread_mutex_lock(&(rx_thread_data.lock));

    printf("%-50s", "Creating RX thread... ");
    status = pthread_create(&rx_thread_pth, NULL, txrx_thread,
                            &rx_thread_data);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", strerror(status));
        enable(&dev, false);
        bladerf_deinit_stream(rx_stream);
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    if(enable(&dev, true)) {
        return 1;
    }

    printf("%-50s", "Unlocking and joining threads... ");
    fflush(stdout);
    pthread_mutex_unlock(&(rx_thread_data.lock));
    pthread_mutex_unlock(&(tx_thread_data.lock));
    pthread_join(rx_thread_pth, NULL);
    pthread_join(tx_thread_pth, NULL);
    printf(KGRN "OK" KNRM "\n");

    printf("%-50s", "All done, checking TX results... ");
    fflush(stdout);
    if(tx_thread_data.rv < 0) {
        printf(KRED "Failed: %s" KNRM "\n",
               bladerf_strerror(tx_thread_data.rv));
        enable(&dev, false);
        bladerf_deinit_stream(rx_stream);
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");


    printf("%-50s", "          checking RX results... ");
    fflush(stdout);
    if(rx_thread_data.rv < 0) {
        printf(KRED "Failed: %s" KNRM "\n",
               bladerf_strerror(tx_thread_data.rv));
        enable(&dev, false);
        bladerf_deinit_stream(rx_stream);
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf(KGRN "Success!" KNRM "\n");

    enable(&dev, false);
    printf("%-50s", "Deinitialising stream... ");
    fflush(stdout);
    bladerf_deinit_stream(rx_stream);
    printf(KGRN "OK" KNRM "\n");
    printf("%-50s", "Closing device... ");
    fflush(stdout);
    bladerf_close(dev);
    printf(KGRN "OK" KNRM "\n");
    return 0;
}
