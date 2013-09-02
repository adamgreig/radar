#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <locale.h>
#include <math.h>
#include "libbladeRF.h"

struct setup_config
{
    unsigned int freq;
    unsigned int bw;
    unsigned int sr;
    int txvga1;
    int txvga2;
    int lna;
    int rxvga1;
    int rxvga2;
};


int setup(struct bladerf** dev, struct setup_config config)
{
    unsigned int abw, asr;
    int status;

    setlocale(LC_NUMERIC, "");

    printf("%-50s", "Connecting to device... ");
    fflush(stdout);
    status = bladerf_open(dev, NULL);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %'13uHz... ", "Tuning TX to:", config.freq);
    fflush(stdout);
    status = bladerf_set_frequency(*dev, BLADERF_MODULE_TX, config.freq);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %'13uHz... ", "Tuning RX to:", config.freq);
    fflush(stdout);
    status = bladerf_set_frequency(*dev, BLADERF_MODULE_RX, config.freq);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %'13uHz... ", "Setting TX bandwidth to:", config.bw);
    fflush(stdout);
    status = bladerf_set_bandwidth(*dev, BLADERF_MODULE_TX, config.bw, &abw);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %'13uHz\n", "Actual bandwidth:", abw);
    if(abw != config.bw) {
        printf("Actual bandwidth not equal to desired bandwidth, quitting.\n");
        return 1;
    }

    printf("%-30s %'13uHz... ", "Setting RX bandwidth to:", config.bw);
    fflush(stdout);
    status = bladerf_set_bandwidth(*dev, BLADERF_MODULE_RX, config.bw, &abw);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %'13uHz\n", "Actual bandwidth:", abw);
    if(abw != config.bw) {
        printf("Actual bandwidth not equal to desired bandwidth, quitting.\n");
        return 1;
    }

    printf("%-30s %'12usps... ", "Setting TX sampling rate to:", config.sr);
    fflush(stdout);
    status = bladerf_set_sample_rate(*dev, BLADERF_MODULE_TX, config.sr, &asr);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %'12usps\n", "Actual sampling rate:", asr);
    if(asr != config.sr) {
        printf("Actual sampling rate not equal to desired sampling rate, quitting.\n");
        return 1;
    }

    printf("%-30s %'12usps... ", "Setting RX sampling rate to:", config.sr);
    fflush(stdout);
    status = bladerf_set_sample_rate(*dev, BLADERF_MODULE_RX, config.sr, &asr);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %'12usps\n", "Actual sampling rate:", asr);
    if(asr != config.sr) {
        printf("Actual sampling rate not equal to desired sampling rate, quitting.\n");
        return 1;
    }

    printf("%-30s %+13ddB... ", "Setting TXVGA1 gain to:", config.txvga1);
    fflush(stdout);
    status = bladerf_set_txvga1(*dev, config.txvga1);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %+13ddB... ", "Setting TXVGA2 gain to:", config.txvga2);
    fflush(stdout);
    status = bladerf_set_txvga2(*dev, config.txvga2);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %+13ddB... ", "Setting RXVGA1 gain to:", config.rxvga1);
    fflush(stdout);
    status = bladerf_set_rxvga1(*dev, config.rxvga1);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %+13ddB... ", "Setting RXVGA2 gain to:", config.rxvga2);
    fflush(stdout);
    status = bladerf_set_rxvga2(*dev, config.rxvga2);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-30s %+15d... ", "Setting LNA gain to:", config.lna);
    fflush(stdout);
    status = bladerf_set_lna_gain(*dev, config.lna);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("All set up.\n");

    return 0;
}

int enable(struct bladerf** dev)
{
    int status;

    printf("%-50s", "Enabling TX... ");
    fflush(stdout);
    status = bladerf_enable_module(*dev, BLADERF_MODULE_TX, true);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    printf("%-50s", "Enabling RX... ");
    fflush(stdout);
    status = bladerf_enable_module(*dev, BLADERF_MODULE_RX, true);
    if(status) {
        printf("\x1B[31mFailed: %s\x1B[0m\n", bladerf_strerror(status));
        bladerf_close(*dev);
        return 1;
    }
    printf("\x1B[32mOK\x1B[0m\n");

    return 0;
}


int main(int argc, char* argv) {
    struct bladerf *dev;
    struct setup_config cfg;
    int result;

    cfg.freq = 3400000000;
    cfg.bw = 28000000;
    cfg.sr = 40000000;
    cfg.txvga1 = -14;  // Post-LPF gain, default -14dB, -35dB to -4dB, 1dB steps
    cfg.txvga2 = 0;    // PA gain,       default   0dB,   0dB to 25dB, 1dB steps
    cfg.rxvga1 = 30;   // Mixer gain,    default  30dB,   5dB to 30dB
    cfg.rxvga2 = 3;    // Post-LPF gain, default   3dB,   0dB to 60dB, 3dB steps
    cfg.lna = BLADERF_LNA_GAIN_MAX;

    if(setup(&dev, cfg)) {
        return 1;
    }

    if(enable(&dev)) {
        return 1;
    }

    printf("Closing down... ");
    bladerf_close(dev);
    printf("OK\n");
    return 0;
}
