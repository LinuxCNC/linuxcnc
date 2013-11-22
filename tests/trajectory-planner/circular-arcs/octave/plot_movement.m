load ~/movement.txt
%Assume 1kHz
v=diff(movement)/0.001;
a=diff(v)/0.001;
plot(movement(:,1),movement(:,2))
