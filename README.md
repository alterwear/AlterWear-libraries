# AlterWear-libraries
Custom EPD libraries for controlling the eink displays from Pervasive Displays.

### Image manipulations
- **image_flip** - This flips the image on the horizontal access (long way) so text is displayed backwards. Can also cycle between patterns w only a single image file.
- **image_fast** - 


### TODO
- [ ] Collect timing info for a single line, and for various sized images (should be the same regardless of image size).
- [ ] See if changing the way line() is called from fixed_data_repeat speeds things up.

### Notes
- Could reduce "stage time" (now: 1.44" and 2" takes 480ms, 2.7" takes 630ms). Will increase ghosting but could speed things up.
- Can skip inverse color steps (image_fast does this), will degrade the display over time.
- **MAJOR PROBLEM**: EPD lib reads data off Arduino progmem. Gets read via pgm_read_byte_near. This might be hard to change..... 
