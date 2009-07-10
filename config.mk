C6_SOURCE = IR77.src

load: ethersex.hex
	avrdude -p m8 -c avrispmkII -P usb -U flash:w:ethersex.hex -B 1

fuse:
	avrdude -p m8 -c avrispmkII -P usb -B 1000 -U lfuse:w:0x2e:m -U hfuse:w:0xd9:m 
