import numpy as np
import matplotlib.pyplot as plt

centre_freq = 3.4E9
sample_rate = 40E6


def db(pwr):
    return 10*np.log10(pwr)

with open("/tmp/bladerf_samples.txt", "rb") as f:
    data = np.fromfile(f, np.int16, -1).reshape((-1, 2)).astype(np.float)

samples = (data[:, 0] + 1j * data[:, 1]) / 2048.0

spectrum = np.fft.fft(samples)
power = np.abs(spectrum)
freqs = np.fft.fftfreq(spectrum.size, 1/sample_rate) + centre_freq

trace = np.abs(samples)
times = np.arange(0, trace.size / sample_rate, 1/sample_rate)

plt.subplot(211)
plt.plot(times, trace, '.')
plt.xlabel("Time (s)")
plt.ylabel("Magnitude")
plt.title("Trace")
plt.grid()

plt.subplot(212)
plt.semilogy(freqs, power, '.')
locs, labels = plt.xticks()
plt.xticks(locs, ["{0:.3f}GHz".format(loc / 1E9) for loc in locs])
locs, labels = plt.yticks()
plt.yticks(locs, ["{0}dB".format(int(db(loc))) for loc in locs])
plt.xlabel("Power")
plt.ylabel("Frequency")

plt.title("Power Spectrum")
plt.grid()

plt.show()
