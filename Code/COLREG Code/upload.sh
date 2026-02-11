# Written by Alexandre DesAulniers, updated for ECED3901. AVRDUDE uploader program. 
#
# ------------------

# To initialize UART pipe in bash, run
#
#`stty -F /dev/ttyUSB0 115200 raw -echo && cat /dev/ttyUSB0`
#
# ------------------
#
# upload.sh requires `avr-gcc avr-binutils avr-libc avrdude` and a C file.
# 
# Use syntax for upload.sh is:
# `./upload.sh file_name.c `


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
# Compile to object file
avr-gcc -Wall -Os -DF_CPU=$F_CPU -mmcu=$MCU -o "$FILENAME.elf" "$1"

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    exit 1
fi

# Convert .elf to .hex
avr-objcopy -O ihex "$FILENAME.elf" "$FILENAME.hex"

echo "--- Uploading $FILENAME.hex to ATmega328P ---"

# Push via avrdude
avrdude -c arduino -p m328p -P $PORT -b $BAUD -U flash:w:"$FILENAME.hex":i

if [ $? -eq 0 ]; then
    echo "Done! Target is running."
    # Cleanup temp files
    rm "$FILENAME.elf" "$FILENAME.hex"
else
    echo "Upload failed!"
    exit 1
fi
