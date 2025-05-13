import csv
import random

# Configuration
filename = 'data.csv'
num_rows = 100  # Change as needed, up to 1024
num_columns = 14

with open(filename, mode='w', newline='') as file:
    writer = csv.writer(file)

    for _ in range(num_rows):
        row = [f"{random.randint(0, 254):03}" for _ in range(num_columns)]
        writer.writerow(row)

print(f"CSV file '{filename}' with {num_rows} rows generated successfully.")

