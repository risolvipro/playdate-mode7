import os
import shlex
import shutil
import argparse
import re

parser = argparse.ArgumentParser(description="Mode7 Resize Tool. ImageMagick is required.")
parser.add_argument('-i', help="input path, it can be an image or a folder. If you pass a folder, filenames should contain a number, e.g. image1.png", required=True)
parser.add_argument('-max', type=int, help="max width", required=True)
parser.add_argument('-min', type=int, help="min width", required=True)
parser.add_argument('-step', type=int, help="step value to decrement max width", required=True)
parser.add_argument('-dither', type=str, help="magick dither type (default is o4x4)", default="o4x4")

def quote_arg(str):
    if os.name == "nt":
        return '"' + str + '"'
    return shlex.quote(str)

args = parser.parse_args()
working_dir = os.getcwd()

input_path = args.i
max_width = args.max
min_width = args.min
step_width = args.step
dither_type = args.dither

src_path = os.path.join(working_dir, input_path)
current_dir = working_dir

if os.path.isabs(input_path):
    src_path = input_path
    current_dir = os.path.dirname(src_path)

assert os.path.exists(src_path), print("Input path does not exist")

table_name = None
src_files = []

if os.path.isdir(src_path):
    # folder
    table_name = os.path.basename(src_path)

    num_files = []
    dir_files = os.listdir(src_path)

    for dir_file in dir_files:
        src_file = os.path.join(src_path, dir_file)
        filename_no_ext = os.path.splitext(dir_file)[0]
        filename_ext = os.path.splitext(dir_file)[1]
        if filename_ext == ".png":
            match = re.search("[0-9]+", filename_no_ext)
            if match:
                extracted_index = int(match[0])
                num_files.append((extracted_index, src_file))
       
    num_files = sorted(num_files, key=lambda file: file[0])
    src_files = [file[1] for file in num_files]
else:
    # single file
    table_name = os.path.splitext(os.path.basename(src_path))[0]
    src_files = [src_path]

table_dir = os.path.join(current_dir, table_name + "-table")

if os.path.exists(table_dir):
    shutil.rmtree(table_dir)
os.mkdir(table_dir)

i = 1
for src_file in src_files:
    
    width = max_width
    while width >= min_width:

        image_path = os.path.join(table_dir, table_name + "-table-" + str(i) + ".png")

        command = "magick " + quote_arg(src_file) + " "
        command += "-resize " + str(width) + " "
        command += "-colorspace gray -ordered-dither " + dither_type + " "
        command += quote_arg(image_path)
            
        os.system(command)

        i += 1
        width -= step_width