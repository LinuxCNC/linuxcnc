select sum(X)'X', sum(Y)'Y', sum(Z)'Z', sum(A)'A', sum(B)'B', sum(C)'C', 
sum(U)'U', sum(V)'V', sum(W)'W' from offsets 
where offsetID in ( :H % 100 + 10000, ( :H - :H % 100) / 100 );