#! /bin/bash -e
rm -rf common common2 sam0
make
openocd -f openocd.cfg  -s C:/cygwin/usr/local/share/openocd/scripts -c "flash_image"
