BitBanging with GPIO
====================

GENERAL USAGE
-------------

Program bitbang accepts no arguments. When ran, it consumes stdin, expecting
one of the following commands per line:

* echo
  Without any redirection, the trimmed string is echoed to stdout. If
  redirected to a file handle, the trimmed string is echoed to that file.
  
* open
  Opens a file and assigns it to the specified file handle.
  
* nanosleep
  Sleeps the specified nanosseconds.

On any error (i.e. invalid command, invalid syntax, invalid file handle, 
invalid filename and so on) the program simply aborts.

  
I-TRIGUE 3300 BASIC USAGE
-------------------------

The following files exists to ease controlling of I-Trigue amplifier:

* alloc (free)
  [to be used alone] Reserves (frees) the needed GPIO for the potentiometers.
  
* turnon (turnoff)
  [to be used alone] Allocs (frees) and raises (lowers) the GPIO associated
  with I-Trigue power-on ("turnoff" does it in reverse order).

* init
  [to be cat'ed to others] Open all the GPIOs and assigns to specific file
  handles:
  * 0: SIMO
  * 1: CLK
  * 2: CS

* cs (¬cs)
  [to be cat'ed to others] Raises (lowers) the level on CS line.

* sckhilo
  [to be cat'ed to others] Does a full clock cycle (clock high -> clock low).

* 0 (1)
  [to be cat'ed to others] Lowers (raises) the level on CS line and does a full
  clock cycle.


I-TRIGUE 3300 USAGE
-------------------

Due to GPIO restrictions, bitbang should be ran with superuser privileges.

* Alloc the potentiometers:
  $ sudo ./bitbang < setup/alloc

* Turn it on:
  $ sudo ./bitbang < switch/turnon

* Send command through SPI (there are 16 'x', which should be either 0 or 1):
  $ (cd pot; cat init ¬cs x x x x x x x x x x x x x x x x cs) | sudo ./bitbang
  
  For "x x x x x x x x x x x x x x x x":
  
  * set bass pot:
    . . 0 1 . . 0 1 <binary value>
          1       1 00-FF
		  
  * set volume pot
    . . 0 1 . . 1 0 <binary value>
          1       2 00-FF
		  
* Turn it off:
  $ sudo ./bitbang < switch/turnoff

* Free the potentiometers:
  $ sudo ./bitbang < setup/free


--------------------------------------------------------------------------------
  BEGIN COPYRIGHT NOTICE
  
  This file is part of program "I-Trigue 2.1 3300 Digital Control"
  Copyright 2013-2014  R. Lemos
  
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
  
  END COPYRIGHT NOTICE

--------------------------------------------------------------------------------
