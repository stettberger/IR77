dnl
dnl aumeIR-B1.m4
dnl
dnl Pin Configuration for 'aumeIR-B1'.  Edit it to fit your needs.
dnl

/* rc5 support */
pin(RC5_SEND, PB5)
RC5_USE_INT(1)

pin(CHANNEL1, PC5, OUTPUT)
pin(CHANNEL2, PC4, OUTPUT)
pin(CHANNEL3, PC3, OUTPUT)
pin(CHANNEL4, PC2, OUTPUT)
pin(CHANNEL5, PC1, OUTPUT)
pin(CHANNEL6, PC0, OUTPUT)
pin(CHANNEL7, PB2, OUTPUT)
pin(CHANNEL8, PB1, OUTPUT)
