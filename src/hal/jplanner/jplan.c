#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_math.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_ring.h"

#include <machinetalk/build/machinetalk/protobuf/jplan.npb.h>
#include <machinetalk/nanopb/pb_decode.h>

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("simple joint planner");
MODULE_LICENSE("GPL");

RTAPI_TAG(HAL,HC_INSTANTIABLE);

static int count = 1;
RTAPI_IP_INT(count, "number of joints per instance");

static int comp_id;
static char *compname = "jplan";

struct joint {
    hal_float_t *pos_cmd;	// position command
    hal_float_t *max_vel;	// velocity limit
    hal_float_t *max_acc;	// acceleration limit

    hal_float_t *curr_pos;	// current position
    hal_float_t *curr_vel;	// current velocity

	hal_float_t *home_pos;	// "home" position : will be used as curr_pos when
	hal_bit_t *home_set;	// home_set is high. Thus enabling homing

    hal_bit_t * active;		// non-zero if motion in progress
    hal_bit_t *enable;		// if zero, motion stops ASAP
};

struct inst_data {
    int count;

    ringbuffer_t jcmd;          // joint command ringbuffer
    hal_bit_t   *joints_active;	// non-zero if any of the joints active
    hal_u32_t   *commands;      // # of commands dequeued
    struct joint joints[0];
};

// per-joint planner. Returns 1 if planner active.
static int update_joint(struct joint *joint,
                        const bool   enable,
                        const double pos_cmd,
                        const double max_vel,
                        const double max_acc,
                        const double home_pos,
                        const bool   home_set,
                        const double period)
{
    double max_dv, tiny_dp, pos_err, vel_req;

    bool active = 0;

    // If home_set is high, pos_cmd will be set to home_pos
    // thus when enabling doing no planning, but enabling homing the output
    // curr-pos value to a new value.
    // n.b. when home_pos and pos-cmd are equal, there will be no activity
    // because there is no position error
    if (home_set) {
        *(joint->curr_pos) = *(joint->home_pos);
    }

    /* compute max change in velocity per servo period */
    max_dv = max_acc * period;
    /* compute a tiny position range, to be treated as zero */
    tiny_dp = max_dv * period * 0.001;
    /* calculate desired velocity */
    if (enable) {
        /* planner enabled, request a velocity that tends to drive
           pos_err to zero, but allows for stopping without position
           overshoot */
        pos_err = pos_cmd - *(joint->curr_pos);
        /* positive and negative errors require some sign flipping to
           avoid rtapi_sqrt(negative) */
        if (pos_err > tiny_dp) {
            vel_req = -max_dv +
                       rtapi_sqrt(2.0 * max_acc * pos_err + max_dv * max_dv);
            /* mark planner as active */
            active = 1;
        } else if (pos_err < -tiny_dp) {
            vel_req =  max_dv -
                       rtapi_sqrt(-2.0 * max_acc * pos_err + max_dv * max_dv);
            /* mark planner as active */
            active = 1;
        } else {
            /* within 'tiny_dp' of desired pos, no need to move */
            vel_req = 0.0;
        }
    } else {
        /* planner disabled, request zero velocity */
        vel_req = 0.0;
        /* and set command to present position to avoid movement when
           next enabled */
        *(joint->pos_cmd) = *(joint->curr_pos);
    }
    /* limit velocity request */
    if (vel_req > max_vel) {
        vel_req = max_vel;
    } else if (vel_req < - max_vel) {
        vel_req = - max_vel;
    }
    /* ramp velocity toward request at accel limit */
    if (vel_req > *(joint->curr_vel) + max_dv) {
        *(joint->curr_vel) += max_dv;
    } else if (vel_req < *(joint->curr_vel) - max_dv) {
        *(joint->curr_vel) -= max_dv;
    } else {
        *(joint->curr_vel) = vel_req;
    }
    /* check for still moving */
    if (*(joint->curr_vel) != 0.0) {
        /* yes, mark planner active */
        active = 1;
    }
    /* integrate velocity to get new position */
    *(joint->curr_pos) += *(joint->curr_vel) * period;

    *(joint->active) = active;
    return active;
}

// thread function, per-instance
// runs through all joints of this instance
static int update(void *arg, const hal_funct_args_t *fa)
{
    struct inst_data *ip = (struct inst_data *) arg;
    double period = ((double) fa_period(fa))*1e-9;

    int i;
    if (ringbuffer_attached(&ip->jcmd)) {

        // fetch next command if all joints inactive
        if (!*(ip->joints_active)) {

            // check for a new command
            void *data;
            ringsize_t size;
            if (record_read(&ip->jcmd, (const void**)&data, &size) == 0) {

                // protobuf-decode it
                pb_istream_t stream = pb_istream_from_buffer(data, size);
                machinetalk_JplanCommand rx =  machinetalk_JplanCommand_init_zero;
                if (!pb_decode(&stream, machinetalk_JplanCommand_fields, &rx)) {
                    rtapi_print_msg(RTAPI_MSG_ERR, "%s: pb_decode(JplanCommand) failed: '%s'",
                                    compname, PB_GET_ERROR(&stream));
                } else {
                    // decode ok - apply all set fields to driving pins
                    for (i = 0; i < rx.joint_count; i++) {
                        struct joint *jp = &ip->joints[i];
                        machinetalk_JplanJoint *jc = &rx.joint[i];
                        if (jc->has_enable) *(jp->enable) = jc->enable;
                        if (jc->has_pos_cmd) *(jp->pos_cmd) = jc->pos_cmd;
                        if (jc->has_max_vel) *(jp->max_vel) = jc->max_vel;
                        if (jc->has_max_acc) *(jp->max_acc) = jc->max_acc;
                    }
                    *(ip->commands) += 1; // count # of queued commands
                }
                // consume record
                record_shift(&ip->jcmd);
            }
        }
    }
    // apply the commanded values, wherever they came from
    // (either directly by setting a pin, or by dequeuing a command
    bool active = 0;
    for (i = 0; i < ip->count; i++) {
        struct joint *jp = &ip->joints[i];
        // plan each joint
        // record if any joint active
        active |= update_joint(jp,
                               *(jp->enable),
                               *(jp->pos_cmd),
                               *(jp->max_vel),
                               *(jp->max_acc),
                               *(jp->home_pos),
                               *(jp->home_set),
                               period);
    }
    *(ip->joints_active) = active;
    return 0;
}


static int instantiate_jplan(const int argc, const char **argv)
{
    const char *name = argv[1];
    struct inst_data *ip;
    int inst_id, i;

    if ((inst_id = hal_inst_create(name, comp_id,
                                   sizeof(struct inst_data) +
                                   count * sizeof(struct joint),
                                   (void **)&ip)) < 0)
        return -1;

    // instance-level objects
    ip->count = count;


    // attach the command ringbuffer '<instancename>.cmd' if it exists
    // must be record mode
    unsigned flags;
    bool queued = 0;
    if (!hal_ring_attachf(&(ip->jcmd), &flags,  "%s.cmd", name)) {
        if ((flags & RINGTYPE_MASK) != RINGTYPE_RECORD) {
            HALERR("ring %s.cmd not a record mode ring: mode=%d",name, flags & RINGTYPE_MASK);
            return -EINVAL;
        }
        ip->jcmd.header->reader = inst_id; // we're the reader - advisory
        if (hal_pin_u32_newf(HAL_OUT, &(ip->commands), inst_id, "%s.commands", name))
            return -1;
        queued = 1;
    }

    // aggregate joint status, 'or' of all <joints>.active
    if (hal_pin_bit_newf(HAL_OUT, &(ip->joints_active), inst_id, "%s.joints-active", name))
        return -1;

    // per-joint objects
    for (i = 0; i < ip->count; i++) {
        struct joint *jp = &ip->joints[i];
        if (hal_pin_bit_newf(HAL_OUT, &(jp->active), inst_id, "%s.%d.active", name, i) ||
            hal_pin_bit_newf(HAL_IN, &(jp->enable), inst_id, "%s.%d.enable", name, i)  ||
            hal_pin_float_newf(HAL_OUT, &(jp->curr_pos), inst_id, "%s.%d.curr-pos", name, i)  ||
            hal_pin_float_newf(HAL_OUT, &(jp->curr_vel), inst_id, "%s.%d.curr-vel", name, i) ||
            hal_pin_float_newf(HAL_IN, &(jp->home_pos), inst_id, "%s.%d.home-pos", name, i)   ||
		    hal_pin_bit_newf(HAL_IN, &(jp->home_set), inst_id, "%s.%d.home-set", name, i))
            return -1;

        hal_pin_dir_t dir = queued ? HAL_OUT : HAL_IN;
        if (hal_pin_float_newf(dir,    &(jp->pos_cmd), inst_id, "%s.%d.pos-cmd", name, i) ||
            hal_pin_float_newf(dir, &(jp->max_vel), inst_id, "%s.%d.max-vel", name, i) ||
            hal_pin_float_newf(dir, &(jp->max_acc), inst_id, "%s.%d.max-acc", name, i))
            return -1;
    }
    hal_export_xfunct_args_t xfunct_args = {
        .type = FS_XTHREADFUNC,
        .funct.x = update,
        .arg = ip,
        .uses_fp = 0,
        .reentrant = 0,
        .owner_id = inst_id
    };
    return hal_export_xfunctf(&xfunct_args, "%s.update", name);
}

static int delete_jplan(const char *name, void *inst, const int inst_size)
{

    struct inst_data *ip = (struct inst_data *) inst;
    int retval;

    if (ringbuffer_attached(&ip->jcmd)) {
        if ((retval = hal_ring_detach(&ip->jcmd)) < 0) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "%s: hal_ring_detach(%s.cmd) failed: %d\n",
                            compname, name, retval);
            return retval;
        }
        ip->jcmd.header->reader = 0;
    }
    return 0;
}

int rtapi_app_main(void)
{
    comp_id = hal_xinit(TYPE_RT, 0, 0,
                        (hal_constructor_t)instantiate_jplan,
                        delete_jplan,
                        compname);
    if (comp_id < 0)
        return comp_id;
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
