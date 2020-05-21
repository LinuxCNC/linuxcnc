import array

def create_axis():
    return array.array('f', [
        # x axis
        1.0,0.0,0.0,
        0.2,1.0,0.2,
        0.0,0.0,0.0,
        0.2,1.0,0.2,
        # y axis
        0.0,1.0,0.0,
        1.0,0.2,0.2,
        0.0,0.0,0.0,
        1.0,0.2,0.2,
        # z axis
        0.0,0.0,1.0,
        0.2,0.2,1.0,
        0.0,0.0,0.0,
        0.2,0.2,1.0,
    ])

def create_box(machine_limit_min, machine_limit_max):
    return array.array('f', [
        machine_limit_min[0], machine_limit_min[1], machine_limit_max[2],
        1.0,0.0,0.0,
        machine_limit_min[0], machine_limit_min[1], machine_limit_min[2],
        1.0,0.0,0.0,

        machine_limit_min[0], machine_limit_min[1], machine_limit_min[2],
        1.0,0.0,0.0,
        machine_limit_min[0], machine_limit_max[1], machine_limit_min[2],
        1.0,0.0,0.0,

        machine_limit_min[0], machine_limit_max[1], machine_limit_min[2],
        1.0,0.0,0.0,
        machine_limit_min[0], machine_limit_max[1], machine_limit_max[2],
        1.0,0.0,0.0,

        machine_limit_min[0], machine_limit_max[1], machine_limit_max[2],
        1.0,0.0,0.0,
        machine_limit_min[0], machine_limit_min[1], machine_limit_max[2],
        1.0,0.0,0.0,


        machine_limit_max[0], machine_limit_min[1], machine_limit_max[2],
        1.0,0.0,0.0,
        machine_limit_max[0], machine_limit_min[1], machine_limit_min[2],
        1.0,0.0,0.0,

        machine_limit_max[0], machine_limit_min[1], machine_limit_min[2],
        1.0,0.0,0.0,
        machine_limit_max[0], machine_limit_max[1], machine_limit_min[2],
        1.0,0.0,0.0,

        machine_limit_max[0], machine_limit_max[1], machine_limit_min[2],
        1.0,0.0,0.0,
        machine_limit_max[0], machine_limit_max[1], machine_limit_max[2],
        1.0,0.0,0.0,

        machine_limit_max[0], machine_limit_max[1], machine_limit_max[2],
        1.0,0.0,0.0,
        machine_limit_max[0], machine_limit_min[1], machine_limit_max[2],
        1.0,0.0,0.0,


        machine_limit_min[0], machine_limit_min[1], machine_limit_min[2],
        1.0,0.0,0.0,
        machine_limit_max[0], machine_limit_min[1], machine_limit_min[2],
        1.0,0.0,0.0,
        
        machine_limit_min[0], machine_limit_max[1], machine_limit_min[2],
        1.0,0.0,0.0,
        machine_limit_max[0], machine_limit_max[1], machine_limit_min[2],
        1.0,0.0,0.0,

        machine_limit_min[0], machine_limit_max[1], machine_limit_max[2],
        1.0,0.0,0.0,
        machine_limit_max[0], machine_limit_max[1], machine_limit_max[2],
        1.0,0.0,0.0,

        machine_limit_min[0], machine_limit_min[1], machine_limit_max[2],
        1.0,0.0,0.0,
        machine_limit_max[0], machine_limit_min[1], machine_limit_max[2],
        1.0,0.0,0.0,]
    )

def create_extents(extents):
    x,y,z,p = 0,1,2,3

    min_extents = extents[0]
    max_extents = extents[1]
        
    pullback = max(max_extents[x] - min_extents[x],
                   max_extents[y] - min_extents[y],
                   max_extents[z] - min_extents[z],
                   2 ) * .1
            
    dashwidth = pullback/4
    zdashwidth = 0
    charsize = dashwidth * 1.5
    halfchar = charsize * .5
        
    """
            if view == z or view == p:
                z_pos = min_extents[z]
                zdashwidth = 0
            else:
                z_pos = min_extents[z] - pullback
                zdashwidth = dashwidth
            """
    x_pos = min_extents[x] - pullback
    y_pos = min_extents[y] - pullback
    z_pos = min_extents[z]

    print(extents)
    
    return array.array('f', [
        #if view != x and max_extents[x] > min_extents[x]:
        
        min_extents[x], y_pos, z_pos,
        1.0, 0.51, 0.53,
        max_extents[x], y_pos, z_pos,
        1.0, 0.51, 0.53,
            
        min_extents[x], y_pos - dashwidth, z_pos - zdashwidth,
        1.0, 0.51, 0.53,
        min_extents[x], y_pos + dashwidth, z_pos + zdashwidth,
        1.0, 0.51, 0.53,
        
        max_extents[x], y_pos - dashwidth, z_pos - zdashwidth,
        1.0, 0.51, 0.53,
        max_extents[x], y_pos + dashwidth, z_pos + zdashwidth,
        1.0, 0.51, 0.53,

            # y dimension
            #if view != y and max_extents[y] > min_extents[y]:
            
        x_pos, min_extents[y], z_pos,
        1.0, 0.51, 0.53,
        x_pos, max_extents[y], z_pos,
        1.0, 0.51, 0.53,
            
        x_pos - dashwidth, min_extents[y], z_pos - zdashwidth,
        1.0, 0.51, 0.53,
        x_pos + dashwidth, min_extents[y], z_pos + zdashwidth,
        1.0, 0.51, 0.53,

        x_pos - dashwidth, max_extents[y], z_pos - zdashwidth,
        1.0, 0.51, 0.53,
        x_pos + dashwidth, max_extents[y], z_pos + zdashwidth,
        1.0, 0.51, 0.53,

        # z dimension
        #if view != z and max_extents[z] > min_extents[z]:
        x_pos, y_pos, min_extents[z],
        1.0, 0.51, 0.53,
        x_pos, y_pos, max_extents[z],
        1.0, 0.51, 0.53,
        
        x_pos - dashwidth, y_pos - zdashwidth, min_extents[z],
        1.0, 0.51, 0.53,
        x_pos + dashwidth, y_pos + zdashwidth, min_extents[z],
        1.0, 0.51, 0.53,
            
        x_pos - dashwidth, y_pos - zdashwidth, max_extents[z],
        1.0, 0.51, 0.53,
        x_pos + dashwidth, y_pos + zdashwidth, max_extents[z],
        1.0, 0.51, 0.53,
    ])  
