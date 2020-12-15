/********************************************************************
* Description: ugenserkins.c
* Test program (originally part of genserkins.c)
* License GPL Version 2
********************************************************************/


#include <stdio.h>    /* ulapi */
#include <sys/time.h> /* struct timeval */
#include "genserkins.h"

static double timestamp()
{
    struct timeval tp;

    if (0 != gettimeofday(&tp, NULL)) {
        return 0.0;
    }
    return ((double) tp.tv_sec) + ((double) tp.tv_usec) / 1000000.0;
}

int main(int argc, char *argv[])
{
#define BUFFERLEN 256
    char buffer[BUFFERLEN];
    int inverse = 1;
    int jacobian = 0;
    EmcPose pos = { {0.0, 0.0, 0.0}, 0.0, 0.0, 0.0 };
    EmcPose vel = { {0.0, 0.0, 0.0}, 0.0, 0.0, 0.0 }; // will need this for
                                                      // jacobian
    double joints[6] = { 0.0 };
    double jointvels[6] = { 0.0 };
    KINEMATICS_INVERSE_FLAGS iflags = 0;
    KINEMATICS_FORWARD_FLAGS fflags = 0;
    int t;
    int retval = 0;
    double start, end;
    int comp_id;
    kparms kp;
    kp.max_joints = GENSER_MAX_JOINTS;
    kp.allow_duplicates = 0;

    comp_id = hal_init("usergenserkins");
    if (genserKinematicsSetup(comp_id,"XYZABC",&kp)) printf("unexpected\n");

    genser_kin_init();

    /* syntax is a.out {i|f # # # # # #} */
    if (argc == 8) {
        if (argv[1][0] == 'f') {
            /* joints passed, so do interations on forward kins for timing */
            for (t = 0; t < 6; t++) {
                if (1 != sscanf(argv[t + 2], "%lf", &joints[t])) {
                    fprintf(stderr, "bad value: %s\n", argv[t + 2]);
                    return 1;
                }
            }
            inverse = 0;
        } else if (argv[1][0] == 'i') {
            /* world coords passed, so do iterations on inverse kins for
               timing */
            if (1 != sscanf(argv[2], "%lf", &pos.tran.x)) {
                fprintf(stderr, "bad value: %s\n", argv[2]);
                return 1;
            }
            if (1 != sscanf(argv[3], "%lf", &pos.tran.y)) {
                fprintf(stderr, "bad value: %s\n", argv[3]);
                return 1;
            }
            if (1 != sscanf(argv[4], "%lf", &pos.tran.z)) {
                fprintf(stderr, "bad value: %s\n", argv[4]);
                return 1;
            }
            if (1 != sscanf(argv[5], "%lf", &pos.a)) {
                fprintf(stderr, "bad value: %s\n", argv[5]);
                return 1;
            }
            if (1 != sscanf(argv[6], "%lf", &pos.b)) {
                fprintf(stderr, "bad value: %s\n", argv[6]);
                return 1;
            }
            if (1 != sscanf(argv[7], "%lf", &pos.c)) {
                fprintf(stderr, "bad value: %s\n", argv[7]);
                return 1;
            }
            inverse = 1;
        } else {
            fprintf(stderr, "syntax: %s {i|f # # # # # #}\n", argv[0]);
            hal_exit(comp_id);
            return 1;
        }
        /* need an initial estimate for the forward kins, so ask for it */
        if (inverse == 0) {
            do {
                printf("initial estimate for Cartesian position, xyzrpw: ");
                fflush(stdout);
                if (NULL == fgets(buffer, BUFFERLEN, stdin)) {
                    hal_exit(comp_id);
                    return 0;
                }
            } while (6 != sscanf(buffer, "%lf %lf %lf %lf %lf %lf",
                    &pos.tran.x, &pos.tran.y, &pos.tran.z, &pos.a, &pos.b, &pos.c));
        }

        start = timestamp();
        if (inverse) {
fprintf(stderr,"hack pos.b bbbbbbbbbbbbbbbbbbbbbbbbb\n");
joints[4]=90;
pos.tran.x=  19.2;
pos.tran.y=   5.5;
pos.tran.z=   9.4;
pos.a     =  92.88;
pos.b     = -90;
pos.c     =  87.12;
fprintf(stderr,"gki0:P %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f\n",
pos.tran.x,pos.tran.y,pos.tran.z,pos.a,pos.b,pos.c);
fprintf(stderr,"gki1:J %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f\n",
joints[0],joints[1],joints[2],joints[3],joints[4],joints[5]);
            retval = genserKinematicsInverse(&pos, joints, &iflags, &fflags);
fprintf(stderr,"gki2:J %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f\n",
joints[0],joints[1],joints[2],joints[3],joints[4],joints[5]);
            if (0 != retval) {
                printf("inv kins error %d <%s>\n", retval,go_result_to_string(retval));
            }
        } else {
            retval = genserKinematicsForward(joints, &pos, &fflags, &iflags);
            if (0 != retval) {
                printf("fwd kins error %d\n", retval);
            }
        }
        end = timestamp();

        printf("calculation time: %f secs\n", (end - start));
        printf("Joints:  0=%8.3f 1=%8.3f 2=%8.3f 3=%8.3f 4=%8.3f 5=%8.3f\n",
               joints[0],joints[1],joints[2], joints[3],joints[4],joints[5]);
        printf("Inverse: x=%8.3f y=%8.3f z=%8.3f a=%8.3f b=%8.3f z=%8.3f\n",
               pos.tran.x,pos.tran.y,pos.tran.z,pos.a,pos.b,pos.c);
        hal_exit(comp_id);
        return 0;
    }

    /* end of if args for timestamping */
    /* else we're interactive, terminate with 'quit'|'exit'|CTRL-D */
    while (!feof(stdin)) {
        if (inverse) {
            if (jacobian) {
                printf("jinv> ");
            } else {
                printf("inv> ");
            }
        } else {
            if (jacobian) {
                printf("jfwd> ");
            } else {
                printf("fwd> ");
            }
        }
        fflush(stdout);

        if (NULL == fgets(buffer, BUFFERLEN, stdin)) {
            break;
        }

        if (buffer[0] == 'i') {
            inverse = 1;
            continue;
        } else if (buffer[0] == 'f') {
            inverse = 0;
            continue;
        } else if (buffer[0] == 'j') {
            jacobian = !jacobian;
            continue;
        } else if (buffer[0] == 'q') { //quit
            break;
        } else if (buffer[0] == 'e') { //exit
            break;
        }

        if (inverse) {
            if (jacobian) {
                if (12 != sscanf(buffer,
                        "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                        &pos.tran.x, &pos.tran.y, &pos.tran.z, &pos.a, &pos.b,
                        &pos.c, &vel.tran.x, &vel.tran.y, &vel.tran.z, &vel.a,
                        &vel.b, &vel.c)) {
                    printf("?\n");
                } else {
//FIXME-AJ
//disabled for now  retval = jacobianInverse(&pos, &vel, joints, jointvels);
                    printf("%f %f %f %f %f %f\n",
                        jointvels[0],
                        jointvels[1],
                        jointvels[2],
                        jointvels[3], jointvels[4], jointvels[5]);
                    if (0 != retval) {
                        printf("inv Jacobian error %d\n", retval);
                    } else {
//FIXME-AJ
//disabled for now      retval = jacobianForward(joints, jointvels, &pos, &vel);
                        printf("%f %f %f %f %f %f\n",
                            vel.tran.x,
                            vel.tran.y, vel.tran.z, vel.a, vel.b, vel.c);
                        if (0 != retval) {
                            printf("fwd kins error %d\n", retval);
                        }
                    }
                }
            } else {
                if (6 != sscanf(buffer, "%lf %lf %lf %lf %lf %lf",
                        &pos.tran.x,
                        &pos.tran.y, &pos.tran.z, &pos.a, &pos.b, &pos.c)) {
                    printf("?\n");
                } else {
fprintf(stderr,"gki1:\n");
                    retval =
                        genserKinematicsInverse(&pos, joints, &iflags, &fflags);
                    printf("%f %f %f %f %f %f\n", joints[0], joints[1],
                        joints[2], joints[3], joints[4], joints[5]);
                    if (0 != retval) {
                        printf("inv kins error %d <%s>\n", retval,go_result_to_string(retval));
                    } else {
                        retval =
                            genserKinematicsForward(joints, &pos, &fflags, &iflags);
                        printf("%f %f %f %f %f %f\n", pos.tran.x, pos.tran.y,
                            pos.tran.z, pos.a, pos.b, pos.c);
                        if (0 != retval) {
                            printf("fwd kins error %d\n", retval);
                        }
                    }
                }
            }
        } else {
            if (jacobian) {
                if (12 != sscanf(buffer,
                        "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf",
                        &joints[0], &joints[1], &joints[2], &joints[3],
                        &joints[4], &joints[5], &jointvels[0], &jointvels[1],
                        &jointvels[2], &jointvels[3], &jointvels[4],
                        &jointvels[5])) {
                    printf("?\n");
                } else {
//FIXME-AJ
//disabled for now  retval = jacobianForward(joints, jointvels, &pos, &vel);
                    printf("%f %f %f %f %f %f\n",
                        vel.tran.x,
                        vel.tran.y, vel.tran.z, vel.a, vel.b, vel.c);
                    if (0 != retval) {
                        printf("fwd kins error %d\n", retval);
                    } else {
//FIXME-AJ
//disabled for now      retval = jacobianInverse(&pos, &vel, joints, jointvels);
                        printf("%f %f %f %f %f %f\n",
                            jointvels[0],
                            jointvels[1],
                            jointvels[2],
                            jointvels[3], jointvels[4], jointvels[5]);
                        if (0 != retval) {
                            printf("inv kins error %d <%s>\n", retval,go_result_to_string(retval));
                        }
                    }
                }
            } else {
                if (6 != sscanf(buffer, "%lf %lf %lf %lf %lf %lf",
                        &joints[0], &joints[1], &joints[2], &joints[3], &joints[4], &joints[5])) {
                    printf("?\n");
                } else {
                    retval = genserKinematicsForward(joints, &pos, &fflags, &iflags);
                    printf("xyzabc: %f %f %f %f %f %f\n",
                          pos.tran.x, pos.tran.y, pos.tran.z, pos.a, pos.b, pos.c);
                    if (0 != retval) {
                        printf("fwd kins error %d\n", retval);
                    } else {
                        retval = genserKinematicsInverse(&pos, joints, &iflags, &fflags);
                        printf("j0--j5: %f %f %f %f %f %f\n",
                              joints[0], joints[1], joints[2], joints[3], joints[4], joints[5]);
                        if (0 != retval) {
                            printf("inv kins error %d <%s>\n", retval,go_result_to_string(retval));
                        }
                    }
                }
            }
        }
    }
    hal_exit(comp_id);

    return 0;

#undef ITERATIONS
#undef BUFFERLEN
}
