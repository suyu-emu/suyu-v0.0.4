#!/bin/bash
# icns_generator.sh    GNU GPLv3 License
# Run this script when a new logo is made and the svg file inside.
# You should install Imagemagick to make the conversions: $brew install imagemagick

# Change working dir to where this script is located.
cd "${0%/*}"

if [ -z $1 ]; then
    echo "icns_generator.sh    GNU GPLv3 License"
    echo "Run this script when a new logo is made and the svg file inside."
    echo ""
    echo "Syntax: ./icns_generator <input.svg>"
    echo ""
    echo "Don't forget to install imagemagick: "
    echo "$ brew install imagemagick"
    exit 0
fi

# Error Handling Stuff: 
## Check command availability
check_command() {
    if ! command -v "$1" &> /dev/null; then
        read -s -n 1 -p "Error: '$1' command not found. Please install $2."
        exit 1
    fi
}

## Convert image with error handling
convert_image() {
    convert -background none -resize "$2" "$1" "$3" || {
        read -s -n 1 -p "Error: Conversion failed for $1"
        exit 1
    }
}

# Check required commands
check_command "convert" "ImageMagick"
check_command "iconutil" "macOS"

# Create the iconset directory
mkdir suyu.iconset || {
    read -s -n 1 -p "Error: Unable to create suyu.iconset directory."
    exit 1
}

# Convert images
convert_image "$1" 16x16 suyu.iconset/icon_16x16.png
convert_image "$1" 32x32 suyu.iconset/icon_16x16@2x.png
convert_image "$1" 32x32 suyu.iconset/icon_32x32.png
convert_image "$1" 64x64 suyu.iconset/icon_32x32@2x.png
convert_image "$1" 128x128 suyu.iconset/icon_128x128.png
convert_image "$1" 256x256 suyu.iconset/icon_256x256.png
convert_image "$1" 256x256 suyu.iconset/icon_128x128@2x.png
convert_image "$1" 512x512 suyu.iconset/icon_256x256@2x.png
convert_image "$1" 512x512 suyu.iconset/icon_512x512.png
convert_image "$1" 1024x1024 suyu.iconset/icon_512x512@2x.png

# Create the ICNS file
iconutil -c icns suyu.iconset || {
    read -s -n 1 -p "Error: Failed to create ICNS file."
    exit 1
}

# Remove the temporary iconset directory
rm -rf suyu.iconset || {
    read -s -n 1 -p "Error: Unable to remove suyu.iconset directory."
    exit 1
}

echo -s -n 1 -p "Icon generation completed successfully."
echo ""
