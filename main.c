#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include "libbladeRF.h"

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"

struct bladerf_config
{
    unsigned int tx_freq; // TX Frequency,   300MHz  to 3.8GHz, in Hz
    unsigned int rx_freq; // RX Frequency,   300MHz  to 3.8GHz, in Hz
    unsigned int bw;      // Bandwidth,      1.5MHz  to  28MHz, in Hz
    unsigned int sr;      // Sample rate,    400kS/s to 40MS/s, in S/s
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

    printf("%-30s %'13uHz... ", "Setting TX bandwidth to:", config.bw);
    fflush(stdout);
    status = bladerf_set_bandwidth(*dev, BLADERF_MODULE_TX, config.bw, &abw);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz\n", "Actual bandwidth:", abw);
    if(abw != config.bw) {
        printf("Actual bandwidth not equal to desired bandwidth, quitting.\n");
        return 1;
    }

    printf("%-30s %'13uHz... ", "Setting RX bandwidth to:", config.bw);
    fflush(stdout);
    status = bladerf_set_bandwidth(*dev, BLADERF_MODULE_RX, config.bw, &abw);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz\n", "Actual bandwidth:", abw);
    if(abw != config.bw) {
        printf("Actual bandwidth not equal to desired bandwidth, quitting.\n");
        return 1;
    }

    printf("%-30s %'12usps... ", "Setting TX sampling rate to:", config.sr);
    fflush(stdout);
    status = bladerf_set_sample_rate(*dev, BLADERF_MODULE_TX, config.sr, &asr);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'12usps\n", "Actual sampling rate:", asr);
    if(asr != config.sr) {
        printf("Actual sampling rate not equal to desired sampling rate, "
               "quitting.\n");
        return 1;
    }

    printf("%-30s %'12usps... ", "Setting RX sampling rate to:", config.sr);
    fflush(stdout);
    status = bladerf_set_sample_rate(*dev, BLADERF_MODULE_RX, config.sr, &asr);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'12usps\n", "Actual sampling rate:", asr);
    if(asr != config.sr) {
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

    if(data->module == BLADERF_MODULE_RX) {
        size_t i;
        int16_t *sample = samples;
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
        data->samples_left -= n_samples;
        if(data->samples_left <= 0) {
            return NULL;
        }
    } else {
    }

    void *rv = data->buffers[data->next_buffer];
    data->next_buffer = (data->next_buffer + 1) % data->num_buffers;
    return rv;
}


int main(int argc, char* argv) {
    struct bladerf *dev;
    struct bladerf_config cfg;
    struct bladerf_stream *stream;
    struct bladerf_stream_data stream_data;
    int status;

    cfg.tx_freq = 3410000000;
    cfg.rx_freq = 3400000000;
    cfg.bw = 28000000;
    cfg.sr = 40000000;
    cfg.txvga1 = -14;
    cfg.txvga2 = 0;
    cfg.rxvga1 = 30;
    cfg.rxvga2 = 3;
    cfg.lna = BLADERF_LNA_GAIN_MAX;

    if(configure_bladerf(&dev, cfg)) {
        return 1;
    }

    printf("%-50s", "Opening /tmp/bladerf_samples.txt...");
    fflush(stdout);
    stream_data.fout = fopen("/tmp/bladerf_samples.txt", "wb");
    if(!stream_data.fout) {
        printf(KRED "Failed: %s" KNRM "\n", strerror(errno));
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    stream_data.num_buffers = 2;
    stream_data.samples_per_buffer = 1048576;   //  1MS
    stream_data.samples_left       = 10485760;  // 10MS
    stream_data.next_buffer = 0;
    stream_data.module = BLADERF_MODULE_RX;

    printf("%-50s", "Initialising data stream... ");
    fflush(stdout);
    status = bladerf_init_stream(&stream, dev, stream_cb,
                                 &stream_data.buffers, stream_data.num_buffers,
                                 BLADERF_FORMAT_SC16_Q12,
                                 stream_data.samples_per_buffer,
                                 stream_data.num_buffers, &stream_data);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    if(enable(&dev, true)) {
        return 1;
    }

    printf("%-50s", "Streaming data... ");
    fflush(stdout);
    status = bladerf_stream(dev, stream_data.module, BLADERF_FORMAT_SC16_Q12,
                            stream);
    if(status < 0) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    enable(&dev, false);
    printf("%-50s", "Deinitialising stream... ");
    fflush(stdout);
    bladerf_deinit_stream(stream);
    printf(KGRN "OK" KNRM "\n");
    printf("%-50s", "Closing device... ");
    fflush(stdout);
    bladerf_close(dev);
    printf(KGRN "OK" KNRM "\n");
    return 0;
}
