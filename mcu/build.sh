
avr-g++ -DF_CPU=16000000 -O2 -mmcu=atmega2560 -o main.o src/main.cpp
avr-objcopy -O ihex main.o main.hex
avrdude -v -p m2560 -c avrispmkII -P /dev/tty$1 -U flash:w:main.hex -D
