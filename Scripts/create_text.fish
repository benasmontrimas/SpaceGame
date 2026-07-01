#!/bin/fish

argparse "f/font=" -- $argv

if set -q _flag_font
        set font $_flag_font[-1]
else
        set font "/home/benas/.local/share/fonts/Google Fonts/Aboreto/Aboreto_Regular.2.ttf"
end

set script_location (status filename)
set output_location (path resolve $script_location/../../Assets/Text)

echo $font

for i in (seq 65 90)
        set name (echo \\(math --base=16 $i | string sub -s 2) | string unescape)
        msdfgen msdf -font $font $i -scale 60 -translate 0 0.2 -emnormalize -pxrange 4 -o $output_location/$name.png
        ktx create $output_location/$name.png $output_location/$name.ktx --format R8G8B8A8_SRGB  --assign-tf linear --convert-tf srgb
end

# msdf-atlas-gen -font Aboreto_Regular.2.ttf -chars "['A', 'Z']" -imageout TitleAtlas.png -dimensions 512 512 -uniformorigin on -size 86.8125 -uniformcell 102 85
# ktx create TitleAtlas.png TitleAtlas.ktx --format R8G8B8A8_SRGB --assign-tf linear --convert-tf srgb