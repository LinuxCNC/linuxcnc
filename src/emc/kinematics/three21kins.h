#ifndef THREE21_H
#define THREE21_H

/* default values for ar2 robot */
#define DEFAULT_THREE21_A1 64.2
#define DEFAULT_THREE21_A2 305.0
#define DEFAULT_THREE21_A3 0.0
#define DEFAULT_THREE21_D1 169.77
#define DEFAULT_THREE21_D2 0.0
#define DEFAULT_THREE21_D3 -6.25
#define DEFAULT_THREE21_D4 223.63
#define DEFAULT_THREE21_D6 36.5

#define SINGULAR_FUZZ 0.000001
#define FLAG_FUZZ     0.000001

/* flags for inverse kinematics */
#define THREE21_SHOULDER_RIGHT 0x01
#define THREE21_ELBOW_DOWN     0x02
#define THREE21_WRIST_FLIP     0x04
#define THREE21_SINGULAR       0x08

/* flags for forward kinematics */
#define THREE21_REACH          0x01

#endif /* THREE21_H */
