## Copyright (C) 2014 Robert W. Ellenberg
## 
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 3 of the License, or
## (at your option) any later version.
## 
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
## 
## You should have received a copy of the GNU General Public License
## along with Octave; see the file COPYING.  If not, see
## <http://www.gnu.org/licenses/>.

## make_peak_img

## Author: Robert W. Ellenberg rob@carbidelabs.com
## Created: 2014-02-18

peaks(200)
colormap(gray(1024))
shading interp
view(0,90)
grid off
print 'peaks.png' '-S


