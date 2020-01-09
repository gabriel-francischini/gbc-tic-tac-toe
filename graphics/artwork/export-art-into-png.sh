#!/bin/bash
for asefile in *.ase
do
    pngfile="${asefile%.*}.png"
    echo "Exporting $asefile into $pngfile ..."
    aseprite -b "$asefile" --save-as "$pngfile" \
             --palette /home/superwolf/aseprite/data/palettes/wp-gbc-tic-tac-toe-gbced.gpl\
        &>>./export_log.log
done
