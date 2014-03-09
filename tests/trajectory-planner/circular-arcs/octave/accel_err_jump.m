## Copyright (C) 2014 Robert Ellenberg
## 
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
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

## accel_err_jump

## Author: Robert Ellenberg rob@carbidelabs.com
## Created: 2014-01-07

function [ a ] = accel_err_jump (v,t_err,t_step)
dp = t_err * v;
p = [0 dp -dp 0];
a = diff(p,2)/t_step^2;
endfunction
