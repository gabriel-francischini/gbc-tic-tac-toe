#!/bin/bash
for asefile in *.ase
do
    pngfile="${asefile%.*}.png"
    echo "Exporting $asefile into $pngfile ..."
    aseprite -b "$asefile" --save-as "$pngfile" &>>./export_log.log
done
