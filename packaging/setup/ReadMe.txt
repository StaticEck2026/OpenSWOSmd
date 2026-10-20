
 OPENSWOS SETUP
 ==============

 OpenSWOS does not include Sensible World of Soccer's graphics, sound or
 team data. OpenSWOS Setup makes them, once, from a copy of the game you
 own:

 - the PC version (from GOG): the installed game's folder
 - the Amiga version: the two disk images (.adf) of SWOS 96/97
 - optional: the Atari Jaguar cartridge image (Sensible Soccer
   International Edition) for the Jaguar sprites, and a SWOS 2020 install
   for its pitches

 This download is the setup on its own, for Windows, macOS and Linux. The
 Windows installer and the game's zips for Windows, Linux and macOS have
 it already; you need this one for the Amiga, or for a game that is on
 another computer.

 Downloads, the other versions and the changelog:
        https://baker76.itch.io/openswos


 RUNNING IT
 ----------

 Unpack the zip as it is: the programs find their files (setup/, data/)
 in the folder above them.

 Windows   Windows\openswos-setup.exe
 macOS     macOS/OpenSWOS Setup.app (Apple silicon and Intel Macs). The
           first time, right-click it and choose Open: it is not signed.
 Linux     Linux/openswos-setup (x86-64), or Linux/swosdata

 Press "Choose a folder..." and pick a folder your game is in: the setup
 looks through it and every folder inside it. Or type, paste or browse
 ("...") a location for each one. Then press Install.

 The data goes where the game looks for it ("Install to" shows it):

   Windows   Documents\SWOS\assets
   macOS     ~/Library/Application Support/SWOS/assets
   Linux     ~/.swos/assets


 THE AMIGA
 ---------

 The setup cannot run on the Amiga; run it on a PC or Mac:

 1. Unpack SWOS-Amiga.zip: it gives you a SWOS drawer.
 2. Install from your Amiga disk images (and the PC version, if you have
    it) as above.
 3. Under "Amiga version", press "Browse..." to choose the SWOS drawer,
    then "Add the game data".
 4. Copy the drawer to your Amiga's hard drive, or to the drive of
    FS-UAE / WinUAE.


 FROM THE COMMAND LINE
 ---------------------

 swosdata does the same without a window:

   swosdata --scan FOLDER                 what it finds there
   swosdata --scan FOLDER --out ASSETS    make the data in ASSETS
   swosdata --installed ASSETS            what an earlier run made

 "Sensible World of Soccer" (C) 1994-97 Sensible Software. OpenSWOS is a
 fan project and is not affiliated with Sensible Software or its current
 rights holders.
