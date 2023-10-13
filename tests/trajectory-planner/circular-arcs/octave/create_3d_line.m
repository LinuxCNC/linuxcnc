function move=create_3d_line(v_range,dp,theta,prev_move,limits)
    %% Create a random trajectory segment based on the input

    if ~isstruct(prev_move)
        %wlog assume +X as initial direction and origin as start
        prev_move.P1=[-1,0,0];
        prev_move.P2=[0,0,0];
    end

        move.P1=prev_move.P2;
    
    %Hard-coded settings for G-code dump, make this more general later if need be
    if length(v_range)==2
        move.v_req=ezrand(v_range(1),v_range(2));
    else
        move.v_req=v_range;
    end

    %dummy value
    move.a_max=1;

    move.tolerance=10;

    dp_old=prev_move.P2-prev_move.P1;
    u_old=dp_old/norm(dp_old);

    if length(theta)==1
        theta_vec=[-theta,theta,-theta,theta,-theta,theta];
    elseif length(theta)==2
        theta_vec=[theta,theta,theta];
        theta_vec=theta_vec(:);
    else
        theta_vec=theta;
    end

    inlimits = false;
    while ~inlimits
        %Rotate about Z 
        theta_z=rand(1)*(theta_vec(2)-theta_vec(1)) + theta_vec(1);
        u_raw=u_old*[cos(theta_z),-sin(theta_z),0;sin(theta_z),cos(theta_z),0;0,0,1];

        %rotate about Y
        theta_y=rand(1)*(theta_vec(4)-theta_vec(3)) + theta_vec(3);
        u_raw=u_raw*[cos(theta_y),0,-sin(theta_y);0,1,0;sin(theta_y),0,cos(theta_y)];

        theta_x=rand(1)*(theta_vec(6)-theta_vec(5)) + theta_vec(5);
        u_raw=u_raw*[1, 0, 0;0, cos(theta_x),sin(theta_x);0,-sin(theta_x),cos(theta_x)];

        %Hack to make a minimum length move
        if length(dp) == 1
            move.P2=move.P1+u_raw*dp;
        else
            move.P2=move.P1+u_raw*(rand(1)*(dp(2)-dp(1))+dp(1));
        end

        if move.P2(1)<limits(2) && move.P2(1)>limits(1) && move.P2(2)<limits(4) && move.P2(2)>limits(3) && move.P2(3)<limits(6) && move.P2(3)>limits(5)
            inlimits = true;
        end
    end

end

