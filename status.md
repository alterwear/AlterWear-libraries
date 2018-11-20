ARIANNA'S PROGRESS.....
----------------------------------------------------------------------------------------------------------

11/19/2018 

TODAY'S PROGRESS:
1. Confirmed that we are reading and writing the correct data to EEPROM. We did this by printing out the data being written to EEPROM in the .ino file, then comparing that to the data being sent to the even_pixels function in the .cpp file to make sure they are the same data. 
2. Understanding the different data representations was a bit of a mind mess, but figured that out... the phone sends data as ASCII characters, which the computer displays as the decimal representation of the ASCII characters. What we really want is not the ASCII, but the actual numbers being sent across, so we convert them using some weird mathy function. The e-ink display takes in the hex representation as input. I made sure that the correctly converted data was being written to EEPROM rather than the weird ASCII version. 

NEXT STEPS:
1. Right now, we are only sending over 16 bytes. The first 9 bytes is always ntag metadata (meaningless to us), so really, we are only sending 7 bytes of useful information right now. A line on the display is 16 bytes, so when we send over the 16 bytes, we only see a single line of random-looking data.
2. Next step is to figure out how to send over more than 16 bytes of data, so that we can see multiple lines on the display.
3. How to do this? Maybe in the alterwear-rx-nfc.ino file in the main loop, we do some kind of for loop where we continuously initializeNFCTransmission() and readfromNFC() over and over again until we've sent over all the info. 
4. Change readFromNFC() to take in a parameter of the address to start reading from, so that we don't overwrite previously written data. We can only send over 16 bytes at a time, so readFromNFC() would start at 0, then 16, then 32... reading and writing 16 bytes at a time. 
5. Learn how to edit the Android app so we have more control over what the phone transfers over to the e-ink display, rather than just the few hardcoded options we have so far. 

Question: The first 9 bytes is always ntag metadata. Does that apply to every single packet of 16 bytes? Or just the very first packet, and then subsequent packet has no more ntag metadata? The latter makes most sense, but we need to check to make sure. 

----------------------------------------------------------------------------------------------------------

11/12/2018 
 
TODAY'S PROGRESS:
1. we confirmed with a sanity check test that the phone is correctly sending data to EEPROM.
2. the phone is sending data as ascii, and so we convert it from ascii to hex to understand what's going on. everything is in binary, but there are many ways of representing data, including ascii, hex, decimal and binary. when working with data that doesn't immediately make sense, it can be helpful to see the data in multiple forms of representation, and perhaps we can make sense of it better more with one way than another. 
- remember that the first 9 bytes of each 16 byte packet is ntag metadata, which is "garbage" to us.

NEXT STEPS: 
figure out how the microcontroller is addressing each byte of data, and whether or not we are reading from the right addresses containing the image information. 

are we passing in the write address to the image?
are we reading from the right addresses when we are stepping through the lines byte by byte? 

then, to confirm, we can try sending image info from the phone to the NFC to see if it displays properly.
to do this, we can grab 800 bytes of data from one of the image .xbm files, and write a script that takes each byte (i.e. "0x00") and only takes the last two numbers and compiles it all into one long file. then we can edit the android app to send in those 800 bytes to the NFC, and hopefully we'll see a fraction of the image show up on the e-ink display.
