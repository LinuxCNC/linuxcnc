% visualize.m the output of the trajectory_example file.
% this file runs with Matlab or octave

T=load('./trajectory.dat');
figure(1);
clf;
plot3(0,0,0);
hold on;
for i=1:size(T,1)
    tf = reshape(T(i,:),4,4)';
    plotframe(tf,0.03);
end
axis equal;grid on;

dt=0.1;
time = (1:length(T(:,4)) )*dt;

figure(2);
subplot(4,1,1);
plot(time,T(:,4));
xlabel('time [s]');
ylabel('position x [m]');

subplot(4,1,2);
plot(time,T(:,8));
xlabel('time [s]');
ylabel('position y [m]');

subplot(4,1,3);
plot(time,T(:,12));
xlabel('time [s]');
ylabel('position z [m]');

subplot(4,1,4);
x=diff(T(:,4));
y=diff(T(:,8));
z=diff(T(:,12));
v=sqrt(x.*x + y.*y + z.*z);
plot(time(1:end-1),v);
xlabel('time [s]');
ylabel('path velocity');