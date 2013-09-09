import json
import numpy as np
import matplotlib.pyplot as plt
import goldcode
import subprocess


origcode = goldcode.generateCACode(1)
code = np.zeros(2048, np.int16)
for idx, bit in enumerate(origcode):
    code[1 + idx*2] = bit
    code[1 + idx*2+1] = bit
code_conj_spec = np.conjugate(np.fft.fft(code))


with open("bladerf_config.json") as f:
    cfg = json.loads(f.read())

centre_freq = float(cfg['rx_freq'])
sample_rate = float(cfg['rx_sr'])


def get_corrs():
    with open("bladerf_samples.dat", "rb") as f:
        data = np.fromfile(f, np.int16, -1).reshape((-1, 2)).astype(np.float)

    samples = (data[:, 0] + 1j * data[:, 1]) / 2048.0
    samples -= np.mean(samples)

    chunks = samples.size // 2048
    corrs = np.zeros((chunks, 2048))
    for idx in range(chunks):
        subsamps = samples[idx*2048:(idx+1)*2048]
        samp_spec = np.fft.fft(subsamps)
        corr_spec = samp_spec * code_conj_spec
        corrs[idx] = np.abs(np.fft.ifft(corr_spec)) ** 2

    avgcorr = np.zeros(2048)

    for corr in corrs:
        #plt.plot(corr, '.-')
        avgcorr += corr

    avgcorr /= chunks

    peakidx = np.argmax(avgcorr)
    avgcorr = np.roll(avgcorr, -(peakidx - 20))
    avgcorr = avgcorr[:50]
    return avgcorr

subprocess.check_call(["./radar"])
corrs = get_corrs()
chart, = plt.plot(corrs, '.-')
plt.xlabel("Code Shift (PRN bits)")
plt.ylabel("Correlation Score")
plt.title("PRN RADAR")
plt.grid()
plt.ylim((0, 150000))
plt.yticks([])
locs, labels = plt.xticks()
plt.xticks(locs, [(float(x)-20.0)/2.0 for x in locs])
plt.show(block=False)
plt.savefig("radar.png", dpi=300)

while True:
    subprocess.check_call(["./radar", "q"])
    corrs = get_corrs()
    chart.set_ydata(corrs)
    plt.draw()
