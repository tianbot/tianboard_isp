# tianboard_isp

tianboard FW updater.

git clone & make

option:  
>-r              reset the board and enter FW update mode  
>-b baudrate     the serial baudrate of the current FW  
>-n baudrate     the serial baudrate of the new FW  
>-f bin file     the firmware bin file to be updated  
>-s serial port  the host serial port connected to the board  
>-v              version  
>-h              help  

Example:
>./tianboard_isp -s /dev/ttyUSB0 -b 115200 -n 115200 -f ../tianboard/build/tianboard.bin -r
