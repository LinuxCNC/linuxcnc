load constraints.log
plot(constraints(:,1:6));
legend('Xacc','Yacc','Zacc','Xvel','Yvel','Zvel','Xpos','Ypos','Zpos');

indX=find(abs(constraints(:,1))>30);
indY=find(abs(constraints(:,2))>30);
indZ=find(abs(constraints(:,3))>30);
