#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# Calculate wasserstein metric and relative error between two distributions
# Usage: python calc_w.py <nq> <qmax> <qdiv> <freq_1> <log_dir_1> [<freq_2> <log_dir_2>]
#   nq: number of quantiles (100, 1000, ...)
#   qmax: only calculate W up to this quantile (99 for 100, 995 for 1000, ...)
#   qdiv: The division of RL (1th to qdivth) and RH (qdiv+1th to qmaxth).
#   freq_1: CPU freq in GHz for the first distribution
#   log_dir_1: data directory for the first distribution
#   freq_2: CPU freq in GHz for the second distribution
#   log_dir_2: data directory for the second distribution
# t_ovh = min(U) - min(V)
# t_min = min(U, V)
# WA_U_V = 1/qmax * sum(abs(U[i] - V[i])) for i = 1, 2, ..., qmax
# For a specific quantile range x,y, W_x_y = 1/(y-x) * sum(abs(U[i] - V[i])) for i = x, x+1, ..., y
# RL = W_1_qdiv / t_min
# RH = W_qdiv_qmax / t_min
# print t_ovh, WA_U_V, RL, RH

import sys
import os
import glob
import numpy as np
import pandas as pd

def load_data(log_dir, freq):
    """Load and process CSV files from directory
    
    Args:
        log_dir: Directory containing CSV files
        freq: CPU frequency for theoretical runtime calculation
        
    Returns:
        Tuple of (theoretical_runtimes, measured_runtimes)
    """
    theoretical_data = []
    measured_data = []
    for csv_file in glob.glob(os.path.join(log_dir, "*.csv")):
        # print(f"Processing file: {csv_file}")
        df = pd.read_csv(csv_file, header=None, names=['rank', 'ntick', 'runtime'])
        theoretical_data.extend((df['ntick']/freq).tolist())
        measured_data.extend(df['runtime'].tolist())
    return np.array(theoretical_data), np.array(measured_data)

def get_quantiles(data, nq):
    """Calculate quantiles of data
    
    Args:
        data: Array of values
        nq: Number of quantiles
        
    Returns:
        Array of quantile values
    """
    return np.percentile(data, np.linspace(0, 100, nq))

def calc_wasserstein(U, V, qmax, qdiv):
    """Calculate Wasserstein metrics between distributions U and V
    
    Args:
        U, V: Arrays containing quantiles of the two distributions
        qmax: Only calculate up to this quantile 
        qdiv: Division point between RL and RH calculations
        
    Returns:
        t_ovh: Overhead time (min(U) - min(V))
        WA: Overall Wasserstein metric
        RL: Relative error for lower quantiles (1 to qdiv)
        RH: Relative error for higher quantiles (qdiv+1 to qmax)
    """
    t_ovh = min(V) - min(U)
    t_min = min(min(U), min(V))
    # Overall Wasserstein metric
    WA = np.sum(np.abs(V[1:qmax+1] - U[1:qmax+1] - t_ovh)) / qmax
    
    # Relative errors for low/high quantiles
    RL = (np.sum(np.abs(V[1:qdiv+1] - U[1:qdiv+1] - t_ovh)) / qdiv) / (sum(U[1:qdiv+1]) / qdiv)
    RH = (np.sum(np.abs(V[qdiv+1:qmax+1] - U[qdiv+1:qmax+1] - t_ovh)) / (qmax-qdiv)) / (sum(U[qdiv+1:qmax+1]) / (qmax-qdiv))
    
    return t_ovh, WA, RL, RH

def main():
    if len(sys.argv) < 6:
        print("Usage: python calc_w.py <nq> <qmax> <qdiv> <freq_1> <log_dir_1> [<freq_2> <log_dir_2>]")
        sys.exit(1)
        
    nq = int(sys.argv[1])
    qmax = int(sys.argv[2])
    qdiv = int(sys.argv[3])
    
    freq1 = float(sys.argv[4])
    dir1 = sys.argv[5]
    
    # Load first distribution
    theoretical_data1, measured_data1 = load_data(dir1, freq1)
    
    if len(sys.argv) > 6:
        # Compare two measured distributions
        freq2 = float(sys.argv[6])
        dir2 = sys.argv[7]
        theoretical_data2, measured_data2 = load_data(dir2, freq2)
        
        U = get_quantiles(measured_data1, nq)
        V = get_quantiles(measured_data2, nq)
    else:
        # Compare theoretical vs measured
        U = get_quantiles(theoretical_data1, nq)
        V = get_quantiles(measured_data1, nq)
    
    t_ovh, WA, RL, RH = calc_wasserstein(U, V, qmax, qdiv)
    print(f"{t_ovh:.6f} {WA:.6f} {RL:.6f} {RH:.6f}")

if __name__ == '__main__':
    main()
