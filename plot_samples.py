import json
import numpy as np
import matplotlib
matplotlib.use('QT4Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import EngFormatter

with open("bladerf_config.json") as f:
    cfg = json.loads(f.read())

centre_freq = float(cfg['rx_freq'])
sample_rate = float(cfg['rx_sr'])


def db(pwr):
    return 10*np.log10(pwr)

with open("bladerf_samples.dat", "rb") as f:
    data = np.fromfile(f, np.int16, -1).reshape((-1, 2)).astype(np.float)

samples = (data[:, 0] + 1j * data[:, 1]) / 2048.0

spectrum = np.fft.fft(samples)
power = np.abs(spectrum)
freqs = np.fft.fftfreq(spectrum.size, 1/sample_rate) + centre_freq

trace = np.abs(samples) / np.sqrt(2)
times = np.arange(0, trace.size / sample_rate, 1/sample_rate)

freq_formatter = EngFormatter(unit='Hz', places=3)
time_formatter = EngFormatter(unit='s', places=3)

ax = plt.subplot(211)
ax.xaxis.set_major_formatter(time_formatter)
plt.plot(times, trace, ',')
plt.xlabel("Time (s)")
plt.ylabel("Magnitude")
plt.title("Trace")
plt.grid()

ax = plt.subplot(212)
ax.xaxis.set_major_formatter(freq_formatter)
plt.semilogy(freqs, power, ',')
locs, labels = plt.xticks()
locs, labels = plt.yticks()
plt.yticks(locs, ["{0}dB".format(int(db(loc))) for loc in locs])
plt.xlabel("Frequency")
plt.ylabel("Power")

plt.title("Power Spectrum")
plt.grid()

plt.show()
