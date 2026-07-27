import math
import argparse

parser = argparse.ArgumentParser(description="Calculate DMA length and transfer times", add_help=False)
parser.add_argument('-w', type=int, help='width of image', default=240)
parser.add_argument('-h', type=int, help='height of image', default=320)
parser.add_argument('--help', action='help')
args = parser.parse_args()


def calculate_max_dma(total):
    max_dma = 0

    # 1. total % 4 == 0
    if total % 4 != 0:
        print("ERROR: total % 4 != 0")
        print("ERROR: total % 4 != 0")
        print("ERROR: total % 4 != 0")
        return max_dma

    # 2. dma_len <= 4092
    c_min = math.ceil(total / 4092.)
    for c in range(c_min, total + 1):
        # 3. total % dma_len == 0
        # 4. dma_len % 4 == 0
        if total % c == 0 and (total / c) % 4 == 0:
            max_dma = total / c
            break

    return max_dma


W = args.w  # width
H = args.h  # height
total = H * (W + 12)  # total
print("total = {} = {} * ({} + 12)".format(total, H, W))

max_dma_value = calculate_max_dma(total)
if max_dma_value != 0:
    print("total = {}, DMA length = {}, Transfer times = {}".format(total, max_dma_value, total / max_dma_value))
else:
    print("Unable to receive images of this width and height")
