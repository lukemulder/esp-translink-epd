#!/bin/bash
SVG_FILES="./svg/*.svg"
PNG_PATH="./png/${1}x${1}"
PNG_FILES="${PNG_PATH}/*.png"
HEADER_PATH="./icons/${1}x${1}"
HEADER="./icons/icons_${1}x${1}.h"

echo "Cleaning old files..."
if [ -e "$PNG_PATH" ];then rm -rf "$PNG_PATH" ; fi
mkdir $PNG_PATH
if [ -e "$HEADER_PATH" ];then rm -rf "$HEADER_PATH" ; fi
mkdir $HEADER_PATH
if [ -e "$HEADER" ];then rm "$HEADER" ; fi


# arguments 1($1) determines the resolution of the output images
# IMAGES MUST HAVE A TOTAL NUMBER OF PIXELS THAT IS DIVISIBLE BY 8
# For sqaure images:
# x = original dimension of icon
# y = desired dimension of icon
# z = density
# In this case we are scaling by 0.25 for better image quality
# ImageMagick default density is 96
# z = 96 * y / (0.25 * x)

for f in $SVG_FILES
do
  echo "Converting .svg to .png for $f..."
  SVG_SIZE=$(identify -format '%w' $f)
  DENSITY=$(bc -l <<< "96 * $1 / $SVG_SIZE")
  mogrify -format png -path $PNG_PATH -colorspace sRGB -density $DENSITY $f
done

for f in $PNG_FILES
do
  echo "Generating header for $f..."
  out="${HEADER_PATH}/$(basename $f .png | tr -s -c [:alnum:] _)${1}x${1}.h"
  python3 png_to_header.py -i $f -o $out
done

echo "Generating include statements..."
echo "#ifndef __ICONS_${1}x${1}_H__" > $HEADER
echo "#define __ICONS_${1}x${1}_H__" >> $HEADER
for f in ${HEADER_PATH}/*.h
do
    echo "#include \"${1}x${1}/$(basename $f)\"" >> $HEADER
done
echo "#endif" >> $HEADER

echo "Done."
