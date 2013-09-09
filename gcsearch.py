import numpy as np
import scipy

G2CodePhsSlct = [
        (2,6), (3,7), (4,8), (5,9), (1,9), (2,10), (1,8),
        (2,9), (3,10), (2,3), (3,4), (5,6), (6,7), (7,8),
        (8,9), (9,10), (1,4), (2,5), (3,6), (4,7), (5,8),
        (6,9), (1,3), (4,6), (5,7), (6,8), (7,9), (8,10),
        (1,6), (2,7), (3,8), (4,9), (5,10), (4,10), (1,7),
        (2,8), (4,10) 
        ]

def binaryAdd(*args):
    """Add 0s and 1s together in GF2"""
    sum = 0
    for arg in args:
        sum += arg

    return sum%2


def generateCACode(PRNid):
    """Return 1023 bit long sattelite PRN code from sattelite id
    
    PRNid - sattelite id number {1,32}

    Returns PRN in numpy array {-1,1} form rather than {0,1}
    """

    # intialise shift registers G1 and G2
    G1 = np.array([1,1,1,1,1,1,1,1,1,1], dtype='int8')
    G2 = np.array([1,1,1,1,1,1,1,1,1,1], dtype='int8')

    # get the correct phase bits for the sat we want
    # then decrement them because python is 0-indexed
    (phsbit1, phsbit2) = G2CodePhsSlct[PRNid-1]
    phsbit1 = phsbit1 - 1
    phsbit2 = phsbit2 - 1


    # initialise empty code
    PRN = np.zeros(1023, dtype='int8')

    # fill up the code
    for i in range(1023):

        # calculate this interation's output bit

        G2ps = binaryAdd(G2[phsbit1], G2[phsbit2])
        G1out = G1[-1]
        PRN[i] = binaryAdd(G2ps, G1out)

        # increment the system state

        newG1 = np.zeros(G1.size)
        # shift and calculate the new first element
        newG1[1:] = G1[:-1]
        newG1[0] = binaryAdd(G1[2], G1[9])

        newG2 = np.zeros(G2.size)
        # shift and calculate the new first element
        newG2[1:] = G2[:-1]
        newG2[0] = binaryAdd(G2[1], G2[2], G2[5], G2[7], G2[8], G2[9])

        G1 = newG1
        G2 = newG2

    # return the PRN mapped from {0,1} to {-1,1}
    return PRN*2 - 1

def PRNCodePhase(PRN, codePhase):
    """ Return the PRN in the specified phase """

    newPRN = np.roll(PRN,codePhase)

    return newPRN

def PRNCodeResample(PRN, sampleFreq=12000000, codeTime=0.001):
    """ Return a resampled PRN for mixing with signal sample"""

    timeBasePRN = codeTime/PRN.size
    timeBaseReSampledPRN = 1.0 / sampleFreq

    reSampledPRN = np.zeros(sampleFreq * codeTime)

    for i,bit in enumerate(reSampledPRN):
        prn_index = int((timeBaseReSampledPRN*i)/timeBasePRN)
        reSampledPRN[i] = PRN[prn_index]
    
    return reSampledPRN


def generateIQ(length, w, sampleFreq = 12000000, phaseOffset=0.0):

    """Return the (I,Q) arrays for the L.O. and the phase of the last
    entry in the array

    length     - length of the array (should be same as sample you're
                 mixing it with
    w          - desired frequency in Hz
    sampleFreq - sample frequency in Hz

    """
    
    index = np.arange(0, length)

    freqnorm = w * (2 * np.pi/sampleFreq)

    wt = freqnorm*index

    I = np.sin(wt + phaseOffset)
    Q = np.cos(wt + phaseOffset)

    finalphase = np.arctan2(np.sin(freqnorm*length + phaseOffset), 
                            np.cos(freqnorm*length + phaseOffset))

    return (I, Q, finalphase)

def pCodePhaseSearch(sample, LOFreq, PRNSpectrumConjugate):

    """ Search in a given LO freq space all the code phases
        at once

    """

    I, Q, _ = generateIQ(sample.size, LOFreq)

    # mix is down with the LO
    sampledMixedI = sample * I
    sampledMixedQ = sample * Q

    # munge them into a single array of complex numbers for the fft
    combinedMixed = sampledMixedI + 1j*sampledMixedQ

    # do the fft
    signalSpectrum = scipy.fft(combinedMixed)

    # circulator correlation in da frequency space
    correlatedSpectrum = signalSpectrum * PRNSpectrumConjugate

    # and back to time domain
    timeDomainReconstructed = np.abs(scipy.ifft(correlatedSpectrum))**2

    return timeDomainReconstructed

def codeParallelSearchForSattelite(sample, PRNid, centreFreq):

    testFreqs = np.arange(centreFreq-10000, centreFreq+10000, 500)
    print "TestFreqs 13th item is: " + str(testFreqs[13])

    results = []
    
    # Make the PRN to test against
    PRN = PRNCodeResample(generateCACode(PRNid))
    PRNSpectrumConjugate = np.conjugate(scipy.fft(PRN))

    # do the test against all the frequency bins
    for idx, freq in enumerate(testFreqs):
        results.append(pCodePhaseSearch(sample, freq, PRNSpectrumConjugate))

    # turn the list of arrays into a 2d array FIXME: cludge
    results = np.array([x for x in results])

    # find the biggest element FIXME: perform a threshold test here
    freq_index, codePhase = np.unravel_index(results.argmax(), results.shape)

    return freq_index, codePhase
