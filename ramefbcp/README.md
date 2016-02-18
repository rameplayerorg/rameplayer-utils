
ramefbcp - Secondary LCD display framebuffer updating for RamePlayer
--------------------------------------------------------------------


TODO:
- animated "buffering" icon
- implement proper clipping to infodisplay drawing
- back and forth auto-scroll feature for too long strings
- support for other than 16bpp fb pixel formats?



3rd party Licenses & Info
-------------------------

Contains code based on SDL2_ttf, with original license here:

  SDL_ttf:  A companion library to SDL for working with TrueType (tm) fonts
  Copyright (C) 2001-2013 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.



ramefbcp.ttf ("Droid Sans") Font License:

Copyright (C) 2008 The Android Open Source Project

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
  
     http://www.apache.org/licenses/LICENSE-2.0
  
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.



Contains code based on code from 'fbcp'. Info from original README:

Raspberry Pi Framebuffer Copy

This program used for copy primary framebuffer to secondary framebuffer (eg. FBTFT). It require lastest raspberry pi firmware (> 2013-07-11) to working properly.

Build

    $ mkdir build
    
    $ cd build
    
    $ cmake ..
    
    $ make 
