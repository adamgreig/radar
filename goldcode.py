import numpy as np

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


def PRNCodeResample(PRN, sampleFreq=12000000, codeTime=0.001):
    """ Return a resampled PRN for mixing with signal sample"""

    timeBasePRN = codeTime/PRN.size
    timeBaseReSampledPRN = 1.0 / sampleFreq

    reSampledPRN = np.zeros(sampleFreq * codeTime)

    for i,bit in enumerate(reSampledPRN):
        prn_index = int((timeBaseReSampledPRN*i)/timeBasePRN)
        reSampledPRN[i] = PRN[prn_index]
    return reSampledPRN


if __name__ == "__main__":
    gc = [str(int(x) * 2047) + ", 0" for x in list(generateCACode(1))]
    print ', '.join(gc)
