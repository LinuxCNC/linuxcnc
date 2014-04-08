function create_3d_random_walk(num_moves, v_range, dp, theta, prefix,limits)
    %% Create a random path for G code testing
    % This version of the code produces random lengths, and random 3d bends. Desired velocity is held at 
    if ~exist('limits','var')
        limits=[-10,10,-10,10,-5,5]
    end


    tag = datestr(now(),"YYYY-MM-DD_HH-MM");
    fsuf=sprintf('%dsteps_%gin-%gin_%gips_%gdeg_max_angle_%s.ngc',num_moves,dp(1),dp(end),v_range(end),max(theta)*180/pi,tag); 
    fid=fopen(sprintf('../nc_files/cv_random_walk_%s_%s',prefix,fsuf),'w');
    %Initial state
    header= 'G61 G90\n G0 X-.1 Y0 Z0 F20\n G1 X0 Y0 Z0\nG64\n';
    fprintf(fid, header);
    move=create_3d_line(v_range, dp, theta,[],limits);
    for k=1:num_moves
        move_prev=move;
        move=create_3d_line(v_range, dp, theta, move_prev, limits);
        fprintf(fid,'G1 X%f Y%f Z%f F%f\n', move_prev.P2(1), move_prev.P2(2), move.P2(3), move_prev.v_req*60);
    end
    fprintf(fid,'M30\n');
    fclose(fid);

end
