#! /usr/bin/octave --persist
load movement.log
%Assume 1kHz
t=movement(:,1);
dt=diff(movement(:,1));
v_nominal=movement(:,3);
dx = [0;diff(movement(:,2))./dt];
dv = [0;diff(dx)./dt];
plot(t,dv)
