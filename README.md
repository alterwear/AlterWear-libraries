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

### How to edit the libraries

1) Download visual code studio using this link: https://code.visualstudio.com/download 
2) Go to the AlterWear-libraries github repository at https://github.com/molecule/AlterWear-libraries
3) Open terminal and cd into your Arduino libraries folder (pathway is Documents/Arduino/libraries)
4) Next, you're going to make a new git repo by typing the following commands into terminal:
```
echo "# hello" >> hello.md
git init
git add hello.md
git commit -m "first commit"
```
5) add the AlterWear-libraries to your empty repository with the following commands:
```
git remote add origin https://github.com/molecule/AlterWear-libraries.git
git pull origin master --allow-unrelated-histories
git log
```
9) Now it's time to edit the .gitignore file. We only care about logging the library files we are editing, but we don't care about all of the other Arduino libraries you have stored in your libraries folder. So, we need to tell git to ignore all of these other libraries. Open the .gitignore file (which is stored in your libraries folder) and copy and paste all of the names of your libraries at the bottom of the .git ignore file, with one library name per line. You can either do it within terminal using vim, or you can open up the file through Finder (.gitignore is usually hidden, but if you press Command + Shift + . then it'll appear in Finder). You can find all these names easily by typing the command "git status" into terminal and git will list out all of the names for you. 
11) 
``` 
git add .
git commit -m "updated .gitignore with more Arduino libraries"
git push --set-upstream origin master
```
