import json
import numpy as np
import subprocess
import matplotlib.pyplot as plt
from matplotlib.ticker import EngFormatter

subprocess.check_call(["./radar"])

with open("bladerf_config.json") as f:
    cfg = json.loads(f.read())

centre_freq = float(cfg['rx_freq'])
sample_rate = float(cfg['rx_sr'])

with open("bladerf_samples.dat", "rb") as f:
    data = np.fromfile(f, np.int16, -1).reshape((-1, 2)).astype(np.float)

samples = (data[:, 0] + 1j * data[:, 1]) / 2048.0

samples -= np.mean(samples)

avgd = np.zeros(256, np.complex128)
chunks = samples.size // 256
for i in range(chunks):
    avgd += samples[i*256:(i+1)*256]
avgd /= float(chunks)
avgd = np.abs(avgd) / np.sqrt(2)
first_peak = np.argmax(avgd)
avgd = np.roll(avgd, -(first_peak - 20))
avgd = avgd[:100]

times = np.linspace(0, avgd.size / sample_rate, avgd.size)

time_formatter = EngFormatter(unit='s', places=3)
ax = plt.axes()
ax.xaxis.set_major_formatter(time_formatter)
chart, = plt.plot(times, avgd, '-')
plt.xlabel("Time (s)")
plt.ylabel("Received Power")
plt.title("Trace")
plt.grid()

plt.show(block=False)

while True:
    subprocess.check_call(["./radar", "q"])
    with open("bladerf_samples.dat", "rb") as f:
        data = np.fromfile(f, np.int16, -1).reshape((-1, 2)).astype(np.float)

    samples = (data[:, 0] + 1j * data[:, 1]) / 2048.0
    samples -= np.mean(samples)
    avgd = np.zeros(256, np.complex128)
    chunks = samples.size // 256
    for i in range(chunks):
        avgd += samples[i*256:(i+1)*256]
    avgd /= float(chunks)
    avgd = np.abs(avgd) / np.sqrt(2)
    first_peak = np.argmax(avgd)
    avgd = np.roll(avgd, -(first_peak - 20))
    avgd = avgd[:100]
    chart.set_ydata(avgd)
    plt.draw()
