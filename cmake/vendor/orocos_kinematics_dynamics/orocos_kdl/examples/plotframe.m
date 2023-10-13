function plotframe(T,scale)
if nargin <2
    scale=0.2;
end


origin=T*[0;0;0;1];
x = T*[scale;0;0;1];
y = T*[0;scale;0;1];
z = T*[0;0;scale;1];

line([origin(1) x(1)],[origin(2) x(2)],[origin(3) x(3)],'color','red','linewidth',2);
line([origin(1) y(1)],[origin(2) y(2)],[origin(3) y(3)],'color','green','linewidth',2);
line([origin(1) z(1)],[origin(2) z(2)],[origin(3) z(3)],'color','blue','linewidth',2);