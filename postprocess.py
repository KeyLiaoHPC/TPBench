import os
import re
import sys
from collections import defaultdict

import pandas as pd
import matplotlib.pyplot as plt


RESULT_DIR = sys.argv[1]
LOG_OUTPUT_DIR = f'{RESULT_DIR}/log'
CSV_OUTPUT_DIR = f'{RESULT_DIR}/csv'
FIG_OUTPUT_DIR = f'{RESULT_DIR}/fig'

os.makedirs(CSV_OUTPUT_DIR, exist_ok=True)
os.makedirs(FIG_OUTPUT_DIR, exist_ok=True)


def extract_numbers(input_string):
    numbers = re.findall(r'\d+\.\d+|\d+', input_string)
    numbers = [float(num) for num in numbers]
    
    return numbers


def add_item(data, lines, i):
    kernel_name = lines[i].split(' ')[-1].strip()

    array_len = None
    bpc_list = None
    while array_len is None or bpc_list is None:
        i += 1
        line = lines[i]
        if "# of Elements" in line:
            array_len = str(int(extract_numbers(line)[0]))
        if "B/c" in line:
            bpc_list = extract_numbers(line)

    mid_bpc, min_bpc, max_bpc = bpc_list[-2], bpc_list[1], bpc_list[-1]
    data[kernel_name]['MID B/c'][array_len] = mid_bpc    
    data[kernel_name]['MIN B/c'][array_len] = min_bpc    
    data[kernel_name]['MAX B/c'][array_len] = max_bpc


def process(input_path):
    print("processing file ", input_path)
    np = int(extract_numbers(input_path)[-1])
    with open(input_path, 'r') as f:
        lines = f.readlines()
        data = defaultdict(lambda: defaultdict(lambda: {}))
        i = 0
        while i < len(lines):
            line = lines[i]
            if ("Running Kernel") in line:
                add_item(data, lines, i)
                i += 1
            else:
                i += 1

        for kernel_name, v in data.items():
            df = pd.DataFrame(v)
            output_csv_path = os.path.join(CSV_OUTPUT_DIR, f'{kernel_name}_np{np:03}.csv')
            df.to_csv(output_csv_path)

            output_fig_path = os.path.join(FIG_OUTPUT_DIR, f'{kernel_name}_np{np:03}.png')
            # plt.scatter(x=df.index, y=df['MID B/c'])
            # plt.scatter(x=df.index, y=df['MIN B/c'])
            # plt.scatter(x=df.index, y=df['MAX B/c'])
            df.plot.line(marker='x')
            plt.ylabel("Bytes/cycle")
            plt.xlabel("array length")
            # plt.xscale('log')
            plt.savefig(output_fig_path)
            plt.clf()
            # plt.show()
            # exit()

for file_name in os.listdir(LOG_OUTPUT_DIR):
    if file_name.endswith('.log'):
        input_path = os.path.join(LOG_OUTPUT_DIR, file_name)
        process(input_path)
