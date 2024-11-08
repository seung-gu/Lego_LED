import numpy as np
import argparse


def convert_symbols_to_binary_2d(filename):
    with open(filename, 'r') as file:
        lines = [line.strip() for line in file.readlines() if line.strip()]  # \n remove

    binary_array = []

    for line in lines:
        binary_row = [1 if char == '#' else 0 for char in line]  # .-> 1, #-> 0
        binary_array.append(binary_row)

    return binary_array


def main(load_path='/home/vis109654/Downloads/bitmap_to_4koled.txt', save_path='/home/vis109654/Downloads/converted.txt'):
    hex_values = np.array(convert_symbols_to_binary_2d(load_path))

    converted_hex_values = []
    for chunk in [hex_values[i:i + 8] for i in range(0, hex_values.shape[0], 8)]:
        # slice the shape 8 chunks and flip it horizontally and transpose
        _chunk = np.fliplr(chunk.T)

        # pad 0 from the left if the number of columns is less than 8
        if _chunk.shape[1] < 8:
            _chunk = np.pad(_chunk, ((0, 0), (8 - _chunk.shape[1], 0)))
        # convert HEX value by using packbits
        converted_hex_values.append([f'0x{np.packbits(row)[0]:02x}' for row in _chunk])

    # save the converted hex values to a file
    with open(save_path, 'w') as file:
        for hex_value in converted_hex_values:
            file.write(', '.join(hex_value)+',\n')


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Converts a bitmap file to a hex file.")
    parser.add_argument('--load', type=str, required=True, help='load path of the text file in bitmap.')
    parser.add_argument('--save', type=str, required=True, help='save path of the hex file.')

    args = parser.parse_args()
    main(args.load, args.save)

