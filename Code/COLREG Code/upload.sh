#!/bin/bash

# Configuration
MCU="atmega328p"
F_CPU="16000000UL"
BAUD="115200"
PORT="/dev/ttyUSB0"

if [ -z "$1" ]; then
    echo "Usage: $0 <filename.c>"
    exit 1
fi

# Strip the extension to get the base name
FILENAME=$(basename "$1" .c)

echo "--- Compiling $1 ---"
# 1. Compile to object file
avr-gcc -Wall -Os -DF_CPU=$F_CPU -mmcu=$MCU -o "$FILENAME.elf" "$1"

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# 2. Convert ELF to HEX
avr-objcopy -O ihex "$FILENAME.elf" "$FILENAME.hex"

echo "--- Uploading $FILENAME.hex to ATmega328P ---"
# 3. Use your existing avrdude command
avrdude -c arduino -p m328p -P $PORT -b $BAUD -U flash:w:"$FILENAME.hex":i

if [ $? -eq 0 ]; then
    echo "Done! Target is running."
    # Cleanup temp files
    rm "$FILENAME.elf" "$FILENAME.hex"
else
    echo "Upload failed!"
    exit 1
fi
