# Binh Nguyen, Oct 2020
# adapted from https://stackoverflow.com/questions/3086973/how-do-i-convert-this-list-of-dictionaries-to-a-csv-file
# 

import csv
import sys
import json

if len(sys.argv) ==2:
	file = sys.argv[1]
	print(f'Processing: {file}')
else:
	print('Need a file name')

with open(file, 'r') as f:
	lines = f.readlines()
lines = [json.loads(line) for line in lines]

print(f'First line:\n{lines[0]}')
keys = lines[0].keys()
save_to = f"{file.split('.')[0]}.csv"
with open(save_to, 'w', newline='')  as f:
    dict_writer = csv.DictWriter(f, keys)
    dict_writer.writeheader()
    dict_writer.writerows(lines)
    print(f'Save {save_to}')