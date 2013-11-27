#! /usr/bin/octave --persist
load movement.log
%Assume 1kHz
dt=diff(movement(:,1));
v=movement(:,3);
a=diff(v)./dt;
plot(movement(:,1),movement(:,2:end))
