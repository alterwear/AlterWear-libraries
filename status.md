11/12/2018

change default text editor in terminal from atom to vim 
so that git commit messages can be typed within terminal without having to open up atom

today's progress:
- we confirmed with a sanity check test that the phone is correctly sending data to EEPROM.
- the phone is sending data as ascii, and so we convert it from ascii to hex to understand what's going on. everything is in binary, but there are many ways of representing data, including ascii, hex, decimal and binary. when working with data that doesn't immediately make sense, it can be helpful to see the data in multiple forms of representation, and perhaps we can make sense of it better more with one way than another. 
- remember that the first 9 bytes of each 16 byte packet is ntag metadata, which is "garbage" to us.

next steps: 
figure out how the microcontroller is addressing each byte of data, and whether or not we are reading from the right addresses containing the image information. 

are we passing in the write address to the image?
are we reading from the right addresses when we are stepping through the lines byte by byte? 

then, to confirm, we can try sending image info from the phone to the NFC to see if it displays properly.
to do this, we can grab 800 bytes of data from one of the image .xbm files, and write a script that takes each byte (i.e. "0x00") and only takes the last two numbers and compiles it all into one long file. then we can edit the android app to send in those 800 bytes to the NFC, and hopefully we'll see a fraction of the image show up on the e-ink display.
