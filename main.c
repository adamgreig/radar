#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <locale.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include "libbladeRF.h"

#define TXFREQ 3410000000
#define TXBW   28000000
#define TXSR   40000000
#define TXVGA1 -4
#define TXVGA2 25

#define RXFREQ 3400000000
#define RXBW   28000000
#define RXSR   40000000
#define RXVGA1 30
#define RXVGA2 30
#define LNA BLADERF_LNA_GAIN_MAX

#define TX_N_BUFFERS          48
#define TX_SAMPLES_PER_BUFFER 2048
#define TX_N_SAMPLES          (280*1024)
#define TX_N_TRANSFERS        32

#define RX_SAMPLES_PER_BUFFER 2048
#define RX_N_SAMPLES          (250*1024)
#define RX_N_BUFFERS          (RX_N_SAMPLES / RX_SAMPLES_PER_BUFFER)
#define RX_N_TRANSFERS        32

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"

char* output_samples_filename = "./bladerf_samples.dat";
char* output_config_filename  = "./bladerf_config.json";

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
    size_t         num_transfers;
    unsigned int   next_buffer;
    bladerf_module module;
    int            samples_left;
};

struct bladerf_thread_data
{
    struct bladerf* dev;
    struct bladerf_stream* stream;
    struct bladerf_stream_data* stream_data;
    int *waiting;
    int rv;
};


int write_config(struct bladerf_config* config)
{
    printf("%-20s %-29s", "Writing config to", output_config_filename);
    fflush(stdout);
    FILE* fout = fopen(output_config_filename, "w");
    if(!fout) {
        printf(KRED "Failed: %s" KNRM "\n", strerror(errno));
        return 1;
    }

    fprintf(fout, "{\n");
    fprintf(fout, "    \"tx_freq\": %u,\n", config->tx_freq);
    fprintf(fout, "    \"rx_freq\": %u,\n", config->rx_freq);
    fprintf(fout, "    \"tx_bw\": %u,\n", config->tx_bw);
    fprintf(fout, "    \"rx_bw\": %u,\n", config->rx_bw);
    fprintf(fout, "    \"tx_sr\": %u,\n", config->tx_sr);
    fprintf(fout, "    \"rx_sr\": %u,\n", config->rx_sr);
    fprintf(fout, "    \"txvga1\": %d,\n", config->txvga1);
    fprintf(fout, "    \"txvga2\": %d,\n", config->txvga2);
    fprintf(fout, "    \"rxvga1\": %d,\n", config->rxvga1);
    fprintf(fout, "    \"rxvga2\": %d\n", config->rxvga2);
    fprintf(fout, "}\n");

    fclose(fout);

    printf(KGRN "OK" KNRM "\n");

    return 0;
}


int configure_bladerf(struct bladerf** dev, struct bladerf_config* config)
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

    printf("%-30s %'13uHz... ", "Tuning TX to:", config->tx_freq);
    fflush(stdout);
    status = bladerf_set_frequency(*dev, BLADERF_MODULE_TX, config->tx_freq);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz... ", "Tuning RX to:", config->rx_freq);
    fflush(stdout);
    status = bladerf_set_frequency(*dev, BLADERF_MODULE_RX, config->rx_freq);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz... ", "Setting TX bandwidth to:", config->tx_bw);
    fflush(stdout);
    status = bladerf_set_bandwidth(*dev, BLADERF_MODULE_TX,
                                   config->tx_bw, &abw);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz\n", "Actual bandwidth:", abw);
    if(abw != config->tx_bw) {
        printf("Actual bandwidth not equal to desired bandwidth, quitting.\n");
        return 1;
    }

    printf("%-30s %'13uHz... ", "Setting RX bandwidth to:", config->rx_bw);
    fflush(stdout);
    status = bladerf_set_bandwidth(*dev, BLADERF_MODULE_RX,
                                   config->rx_bw, &abw);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'13uHz\n", "Actual bandwidth:", abw);
    if(abw != config->rx_bw) {
        printf("Actual bandwidth not equal to desired bandwidth, quitting.\n");
        return 1;
    }

    printf("%-30s %'12usps... ", "Setting TX sampling rate to:",
           config->tx_sr);
    fflush(stdout);
    status = bladerf_set_sample_rate(*dev, BLADERF_MODULE_TX,
                                     config->tx_sr, &asr);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'12usps\n", "Actual sampling rate:", asr);
    if(asr != config->tx_sr) {
        printf("Actual sampling rate not equal to desired sampling rate, "
               "quitting.\n");
        return 1;
    }

    printf("%-30s %'12usps... ", "Setting RX sampling rate to:",
           config->rx_sr);
    fflush(stdout);
    status = bladerf_set_sample_rate(*dev, BLADERF_MODULE_RX,
                                     config->rx_sr, &asr);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %'12usps\n", "Actual sampling rate:", asr);
    if(asr != config->rx_sr) {
        printf("Actual sampling rate not equal to desired sampling rate, "
               "quitting.\n");
        return 1;
    }

    printf("%-30s %+13ddB... ", "Setting TXVGA1 gain to:", config->txvga1);
    fflush(stdout);
    status = bladerf_set_txvga1(*dev, config->txvga1);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %+13ddB... ", "Setting TXVGA2 gain to:", config->txvga2);
    fflush(stdout);
    status = bladerf_set_txvga2(*dev, config->txvga2);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %+13ddB... ", "Setting RXVGA1 gain to:", config->rxvga1);
    fflush(stdout);
    status = bladerf_set_rxvga1(*dev, config->rxvga1);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %+13ddB... ", "Setting RXVGA2 gain to:", config->rxvga2);
    fflush(stdout);
    status = bladerf_set_rxvga2(*dev, config->rxvga2);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-30s %15d... ", "Setting LNA gain to:", config->lna);
    fflush(stdout);
    status = bladerf_set_lna_gain(*dev, config->lna);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("All set up.\n");

    return 0;
}


int enable(struct bladerf* dev, bool enabled)
{
    int status;

    if(enabled)
        printf("%-50s", "Enabling TX... ");
    else
        printf("%-50s", "Disabling TX... ");
    fflush(stdout);
    status = bladerf_enable_module(dev, BLADERF_MODULE_TX, enabled);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    if(enabled)
        printf("%-50s", "Enabling RX... ");
    else
        printf("%-50s", "Disabling RX... ");
    fflush(stdout);
    status = bladerf_enable_module(dev, BLADERF_MODULE_RX, enabled);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(dev);
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

    data->samples_left -= n_samples;
    if(data->samples_left <= 0) {
        return NULL;
    }

    void *rv = data->buffers[data->next_buffer];
    data->next_buffer = (data->next_buffer + 1) % data->num_buffers;
    return rv;
}


int setup_rx_stream(struct bladerf* dev, struct bladerf_stream** stream,
                    struct bladerf_stream_data* stream_data)
{
    int status;

    stream_data->next_buffer = 0;
    stream_data->module = BLADERF_MODULE_RX;

    printf("%-50s", "Initialising RX data stream... ");
    fflush(stdout);
    status = bladerf_init_stream(stream, dev, stream_cb,
                                 &stream_data->buffers,
                                 stream_data->num_buffers,
                                 BLADERF_FORMAT_SC16_Q12,
                                 stream_data->samples_per_buffer,
                                 stream_data->num_transfers, stream_data);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(dev);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");
    return 0;
}


void fill_tx_buffers(struct bladerf_stream_data* stream_data)
{
    size_t i, j;
    int16_t* samples;
    for(i=0; i<stream_data->num_buffers; i++) {
        samples = stream_data->buffers[i];
        memset(samples, 0,
               stream_data->samples_per_buffer * sizeof(*samples) * 2);
        for(j=0; j<(stream_data->samples_per_buffer / 256); j++) {
            samples[j*2*256] = 2047;
        }
    }
}


int setup_tx_stream(struct bladerf* dev, struct bladerf_stream** stream,
                    struct bladerf_stream_data* stream_data)
{
    int status;

    stream_data->next_buffer = 0;
    stream_data->module = BLADERF_MODULE_TX;

    printf("%-50s", "Initialising TX data stream... ");
    fflush(stdout);
    status = bladerf_init_stream(stream, dev, stream_cb,
                                 &stream_data->buffers,
                                 stream_data->num_buffers,
                                 BLADERF_FORMAT_SC16_Q12,
                                 stream_data->samples_per_buffer,
                                 stream_data->num_transfers, stream_data);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", bladerf_strerror(status));
        bladerf_close(dev);
        return 1;
    }

    fill_tx_buffers(stream_data);

    printf(KGRN "OK" KNRM "\n");
    return 0;
}


void* txrx_thread(void* arg)
{
    struct bladerf_thread_data* thread_data = (struct bladerf_thread_data*)arg;

    /*
    while(*(thread_data->waiting))
        usleep(1);
    */

    thread_data->rv = bladerf_stream(thread_data->stream,
                                     thread_data->stream_data->module);

    return NULL;
}


int save_rx_data(struct bladerf_stream_data* stream_data)
{
    FILE* fout;
    size_t i, j, written;
    int16_t* samples;

    printf("%-10s %-39s", "Opening", output_samples_filename);
    fflush(stdout);
    fout = fopen(output_samples_filename, "wb");
    if(!fout) {
        printf(KRED "Failed: %s" KNRM "\n", strerror(errno));
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-50s", "Writing data... ");

    for(i=0; i<stream_data->num_buffers; i++) {
        samples = stream_data->buffers[i];
        for(j=0; j<stream_data->samples_per_buffer; j++) {
            *samples &= 0x0fff;
            if(*samples & 0x0800)
                *samples |= 0xf000;
            *(samples+1) &= 0x0fff;
            if(*(samples+1) & 0x0800)
                *(samples+1) |= 0xf000;
            samples += 2;
        }
        samples = stream_data->buffers[i];

        written = fwrite(samples, sizeof(*samples),
                         stream_data->samples_per_buffer * 2, fout);
        if(written != stream_data->samples_per_buffer * 2) {
            printf(KRED "Failed: %s" KNRM "\n", strerror(errno));
        }

    }


    printf(KGRN "OK" KNRM "\n");

    fclose(fout);

    return 0;
}

void ignore_sigint(int sig)
{
    return;
}


int main(int argc, char** argv) {
    int status, threads_waiting = 1, quick;
    int16_t* rx_data;
    struct bladerf* dev;
    struct bladerf_config* cfg;
    struct bladerf_stream* tx_stream;
    struct bladerf_stream* rx_stream;
    struct bladerf_stream_data* tx_stream_data;
    struct bladerf_stream_data* rx_stream_data;
    struct bladerf_thread_data* tx_thread_data;
    struct bladerf_thread_data* rx_thread_data;
    pthread_t tx_thread_pth;
    pthread_t rx_thread_pth;

    signal(SIGINT, ignore_sigint);

    cfg = malloc(sizeof(struct bladerf_config));
    tx_stream_data = malloc(sizeof(struct bladerf_stream_data));
    rx_stream_data = malloc(sizeof(struct bladerf_stream_data));
    tx_thread_data = malloc(sizeof(struct bladerf_thread_data));
    rx_thread_data = malloc(sizeof(struct bladerf_thread_data));

    cfg->tx_freq = TXFREQ;
    cfg->rx_freq = RXFREQ;
    cfg->tx_bw = TXBW;
    cfg->rx_bw = RXBW;
    cfg->tx_sr = TXSR;
    cfg->rx_sr = RXSR;
    cfg->txvga1 = TXVGA1;
    cfg->txvga2 = TXVGA2;
    cfg->rxvga1 = RXVGA1;
    cfg->rxvga2 = RXVGA2;
    cfg->lna = LNA;

    if(argc == 2 && argv[1][0] == 'q') {
        quick = 1;
    } else {
        quick = 0;
    }

    if(!quick) {
        if(write_config(cfg)) {
            if(cfg) free(cfg);
            if(tx_stream_data) free(tx_stream_data);
            if(rx_stream_data) free(rx_stream_data);
            if(tx_thread_data) free(tx_thread_data);
            if(rx_thread_data) free(rx_thread_data);
            return 1;
        }

        if(configure_bladerf(&dev, cfg)) {
            if(cfg) free(cfg);
            if(tx_stream_data) free(tx_stream_data);
            if(rx_stream_data) free(rx_stream_data);
            if(tx_thread_data) free(tx_thread_data);
            if(rx_thread_data) free(rx_thread_data);
            return 1;
        }
    } else {
        status = bladerf_open(&dev, NULL);
        if(status) {
            return 1;
        }
    }

    tx_stream_data->num_buffers        = TX_N_BUFFERS;
    tx_stream_data->samples_per_buffer = TX_SAMPLES_PER_BUFFER;
    tx_stream_data->samples_left       = TX_N_SAMPLES;
    tx_stream_data->num_transfers      = TX_N_TRANSFERS;

    if(setup_tx_stream(dev, &tx_stream, tx_stream_data)) {
        if(cfg) free(cfg);
        if(tx_stream_data) free(tx_stream_data);
        if(rx_stream_data) free(rx_stream_data);
        if(tx_thread_data) free(tx_thread_data);
        if(rx_thread_data) free(rx_thread_data);
        return 1;
    }

    rx_stream_data->num_buffers        = RX_N_BUFFERS;
    rx_stream_data->samples_per_buffer = RX_SAMPLES_PER_BUFFER;
    rx_stream_data->samples_left       = RX_N_SAMPLES;
    rx_stream_data->num_transfers      = RX_N_TRANSFERS;

    if(setup_rx_stream(dev, &rx_stream, rx_stream_data)) {
        bladerf_deinit_stream(tx_stream);
        if(cfg) free(cfg);
        if(tx_stream_data) free(tx_stream_data);
        if(rx_stream_data) free(rx_stream_data);
        if(tx_thread_data) free(tx_thread_data);
        if(rx_thread_data) free(rx_thread_data);
        return 1;
    }

    if(enable(dev, true)) {
        enable(dev, false);
        bladerf_deinit_stream(rx_stream);
        bladerf_deinit_stream(tx_stream);
        bladerf_close(dev);
        if(cfg) free(cfg);
        if(tx_stream_data) free(tx_stream_data);
        if(rx_stream_data) free(rx_stream_data);
        if(tx_thread_data) free(tx_thread_data);
        if(rx_thread_data) free(rx_thread_data);
        return 1;
    }

    tx_thread_data->dev = dev;
    tx_thread_data->stream = tx_stream;
    tx_thread_data->stream_data = tx_stream_data;
    tx_thread_data->waiting = &threads_waiting;
    tx_thread_data->rv = 1;

    printf("%-50s", "Creating TX thread... ");
    status = pthread_create(&tx_thread_pth, NULL, txrx_thread,
                            tx_thread_data);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", strerror(status));
        bladerf_deinit_stream(rx_stream);
        bladerf_close(dev);
        if(cfg) free(cfg);
        if(tx_stream_data) free(tx_stream_data);
        if(rx_stream_data) free(rx_stream_data);
        if(tx_thread_data) free(tx_thread_data);
        if(rx_thread_data) free(rx_thread_data);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    usleep(100);

    rx_thread_data->dev = dev;
    rx_thread_data->stream = rx_stream;
    rx_thread_data->stream_data = rx_stream_data;
    rx_thread_data->waiting = &threads_waiting;
    rx_thread_data->rv = 1;

    printf("%-50s", "Creating RX thread... ");
    status = pthread_create(&rx_thread_pth, NULL, txrx_thread,
                            rx_thread_data);
    if(status) {
        printf(KRED "Failed: %s" KNRM "\n", strerror(status));
        bladerf_deinit_stream(rx_stream);
        bladerf_deinit_stream(tx_stream);
        bladerf_close(dev);
        if(cfg) free(cfg);
        if(tx_stream_data) free(tx_stream_data);
        if(rx_stream_data) free(rx_stream_data);
        if(tx_thread_data) free(tx_thread_data);
        if(rx_thread_data) free(rx_thread_data);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf("%-50s", "Starting processing... ");
    threads_waiting = 0;
    printf(KGRN "OK" KNRM "\n");

    printf("%-50s", "Waiting for completion... ");
    fflush(stdout);
    pthread_join(rx_thread_pth, NULL);
    pthread_join(tx_thread_pth, NULL);
    printf(KGRN "OK" KNRM "\n");

    printf("%-50s", "All done, checking TX results... ");
    fflush(stdout);
    if(tx_thread_data->rv < 0) {
        printf(KRED "Failed: %s" KNRM "\n",
               bladerf_strerror(tx_thread_data->rv));
        enable(dev, false);
        bladerf_deinit_stream(rx_stream);
        bladerf_close(dev);
        if(cfg) free(cfg);
        if(tx_stream_data) free(tx_stream_data);
        if(rx_stream_data) free(rx_stream_data);
        if(tx_thread_data) free(tx_thread_data);
        if(rx_thread_data) free(rx_thread_data);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");


    printf("%-50s", "          checking RX results... ");
    fflush(stdout);
    if(rx_thread_data->rv < 0) {
        printf(KRED "Failed: %s" KNRM "\n",
               bladerf_strerror(tx_thread_data->rv));
        enable(dev, false);
        bladerf_deinit_stream(rx_stream);
        bladerf_close(dev);
        if(cfg) free(cfg);
        if(tx_stream_data) free(tx_stream_data);
        if(rx_stream_data) free(rx_stream_data);
        if(tx_thread_data) free(tx_thread_data);
        if(rx_thread_data) free(rx_thread_data);
        return 1;
    }
    printf(KGRN "OK" KNRM "\n");

    printf(KGRN "Success!" KNRM "\n");

    save_rx_data(rx_stream_data);

    enable(dev, false);
    printf("%-50s", "Deinitialising stream... ");
    fflush(stdout);
    bladerf_deinit_stream(rx_stream);
    printf(KGRN "OK" KNRM "\n");
    printf("%-50s", "Closing device... ");
    fflush(stdout);
    bladerf_close(dev);
    printf(KGRN "OK" KNRM "\n");

    printf("%-50s", "Freeing memory... ");
    fflush(stdout);
    if(cfg) free(cfg);
    if(tx_stream_data) free(tx_stream_data);
    if(rx_stream_data) free(rx_stream_data);
    if(tx_thread_data) free(tx_thread_data);
    if(rx_thread_data) free(rx_thread_data);
    if(rx_data) free(rx_data);
    printf(KGRN "OK" KNRM "\n");
    return 0;
}
