import numpy as np
import matplotlib.pyplot as plt
from scipy import signal

# Given component values
R = 1000.0        # Ohms
L = 0.0005        # Henry (0.5 mH)
C = 10e-6         # Farads (10 uF)

# Transfer function H(s) = (R * s * C) / (L*C*s^2 + R*C*s + 1)
num = [R * C, 0]       # Coefficient for s: 1000 * 10e-6 = 0.01, so numerator is 0.01*s
den = [L * C, R * C, 1]  # Denom: L*C = 0.0005*10e-6 = 5e-9, R*C = 0.01, constant = 1

# Create the transfer function system
system = signal.TransferFunction(num, den)

# Define frequency range for the bode plot (in rad/s)
w = np.logspace(0, 8, 1000)  # from 1 rad/s to 1e8 rad/s

# Get magnitude (in dB) and phase (in degrees)
w, mag, phase = signal.bode(system, w=w)

# Create the plots
plt.figure(figsize=(8, 6))

# Magnitude plot
plt.subplot(2, 1, 1)
plt.semilogx(w, mag, 'b')
plt.title('Bode Plot of the Passive Bandpass Filter')
plt.ylabel('Magnitude (dB)')
plt.grid(True, which='both', ls='--')

# Phase plot
plt.subplot(2, 1, 2)
plt.semilogx(w, phase, 'r')
plt.xlabel('Frequency (rad/s)')
plt.ylabel('Phase (degrees)')
plt.grid(True, which='both', ls='--')

plt.tight_layout()
plt.show()
