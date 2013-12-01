function depth = estimate_depth(a_limit,v_limit,L_min,dt)

    v_sample= L_min/(2.0*dt);
    v_max = min(v_sample,v_limit);
    v = 0;
    depth = 0;
    while v<v_max
        v = sqrt(v^2 + 2*a_limit*L_min);
        depth+=1;
    end
    
end
