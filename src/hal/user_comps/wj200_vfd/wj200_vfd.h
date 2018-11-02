
#ifndef WJ200_VFD_H
#define WJ200_VFD_H

#define WJ200_VERSION_MAJOR        0x00
#define WJ200_VERSION_MINOR        0x02
/* WJ200 modbus registers source user manual v3.1.3.1 04/2016         */

/* read/write registers, adresses correspond to UM (NOT decremented)  */
#define WJ200_FREQ_SRC_CHZ         0x0000

/* read only monitoring registers, adresses correspond to UM but have 
 * been decremented by 1 */

#define WJ200_FREQ_MON_CHZ         0x1000
#define WJ200_CURRENT_MON_CA       0x1002
#define WJ200_ROTATING_MON         0x1003

#define WJ200_TORQUE_MON           0x100F
#define WJ200_VOLT_MON_DV          0x1010
#define WJ200_POWER_MON_DKW        0x1011

#define WJ200_HEAT_SINK_TEMP_DC    0x1018

#define WJ200_DC_VOLTAGE_PN_DV     0x1025

#define WJ200_INVERTER_MODE        0x1056

#define WJ200_ACCEL_TIME_CS        0x1102
#define WJ200_DECEL_TIME_CS        0x1104

#define WJ200_FREQ_SOURCE          0x1200
#define WJ200_RUNCMD_SOURCE        0x1201
#define WJ200_FREQ_BASE_DHZ        0x1202
#define WJ200_FREQ_MAX_DHZ         0x1203

#define WJ200_FREQ_UPPER_LIMIT_CHZ 0x124E
#define WJ200_FREQ_LOWER_LIMIT_CHZ 0x1250

#define WJ200_MOTOR_VOLTAGE_CLASS  0x1269

#define WJ200_MOTOR_POWER          0x1502
#define WJ200_MOTOR_POLES          0x1503

#define WJ200_COM_WATCHDOG_CS      0x162D
#define WJ200_COM_ERROR_ACTION     0x162E

/* WJ200 modbus coils source user manual v3.1.3.1 04/2016             */
/* adresses correspond to UM but have been decremented by 1           */

/* read/write */
#define WJ200_CMD_RUN              0x0000   
#define WJ200_CMD_ROTATION_REV     0x0001   
#define WJ200_CMD_EXT_ERR          0x0002   
#define WJ200_CMD_RESET_ERR        0x0003   

/* read only monitoring coils adresses */
#define WJ200_OPERATION_STATUS     0x000E   //1:run, 0:stop
#define WJ200_ROTATION_DIRECTION   0x000F   //1:reverse, 0:forward
#define WJ200_INVERTER_READY       0x0010   //1:ready, 0:not ready

#define WJ200_RUNNING              0x0012   //1:tripping, 0:normal
#define WJ200_CONST_SPD_REACHED    0x0013   //1:on, 0:off
#define WJ200_SET_SPD_OVER         0x0014   //1:on, 0:off
#define WJ200_CURRENT_OVERLOAD_ADV 0x0015   //1:on, 0:off

#define WJ200_ALARM                0x0017   //1:on, 0:off
#define WJ200_SET_SPD_REACHED      0x0018   //1:on, 0:off

/* wj200_params_static_t : not changing params eg read once */
typedef struct
{
    uint8_t inverter_mode;
    float   accel_time_s;
    float   decel_time_s;
    uint8_t freq_source;
    uint8_t runcmd_source;
    float   freq_max_hz;
    float   freq_base_hz;  
    float   freq_upper_limit_hz;
    float   freq_lower_limit_hz;  
    uint8_t mot_voltage_class;
    uint8_t mot_power;
    uint8_t mot_poles;
    float   com_watchdog_s;
    uint8_t com_error_action;
} wj200_params_static_t; 

/* wj200_params_dynamic_t */
typedef struct
{
    uint8_t running;
    uint8_t direction_rev;
    uint8_t ready;
    uint8_t running_mode;
    uint8_t cst_speed_reached;
    uint8_t current_overload_adv;
    uint8_t alarm;
    float freq_src_hz;
    float freq_mon_hz;
    uint8_t rotating_mon;
    float current_mon_a;
    int16_t torque_mon;
    float voltage_mon_v;
    float power_mon_kw;
    float heat_sink_temp_c;
    float dc_voltage_pn_v;
} wj200_params_dynamic_t;

#define params_static_read_retry_max     3
#define params_dynamic_read_retry_max    3

/* wj200_instance_state */
typedef struct
{
    bool    bparams_static_read_done;
    uint8_t uparams_static_read_retry;
    bool    bparams_dynamic_read_done;
    uint8_t uparams_dynamic_read_retry;
    bool    bmbslaveset;
    bool    binstance_failure;
    bool    bexit_request_done;
    
} wj200_instance_state_t;


#endif /* WJ200_VFD_H */
