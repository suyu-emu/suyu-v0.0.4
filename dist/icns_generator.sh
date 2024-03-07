mkdir suyu.iconset
convert  -background none -resize 16x16 suyu.svg suyu.iconset/icon_16x16.png;
convert  -background none -resize 32x32 suyu.svg suyu.iconset/icon_16x16@2x.png;
convert  -background none -resize 32x32 suyu.svg suyu.iconset/icon_32x32.png;
convert  -background none -resize 64x64 suyu.svg suyu.iconset/icon_32x32@2x.png;
convert  -background none -resize 128x128 suyu.svg suyu.iconset/icon_128x128.png;
convert  -background none -resize 256x256 suyu.svg suyu.iconset/icon_256x256.png;
convert  -background none -resize 256x256 suyu.svg suyu.iconset/icon_128x128@2x.png;
convert  -background none -resize 512x512 suyu.svg suyu.iconset/icon_256x256@2x.png;
convert  -background none -resize 512x512 suyu.svg suyu.iconset/icon_512x512.png;
convert  -background none -resize 1024x1024 suyu.svg suyu.iconset/icon_512x512@2x.png;

iconutil -c icns suyu.iconset
rm -rf suyu.iconset
