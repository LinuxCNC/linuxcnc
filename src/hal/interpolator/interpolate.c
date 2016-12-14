#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_math.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_ring.h"

#include <machinetalk/build/machinetalk/protobuf/ros.npb.h>
#include <machinetalk/nanopb/pb_decode.h>

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("generic interpolator");
MODULE_LICENSE("GPL");

RTAPI_TAG(HAL,HC_INSTANTIABLE);

static int count = 1;
RTAPI_IP_INT(count, "number of joints per instance");

static int comp_id;
static char *compname = "interpolate";

struct joint {
    hal_float_t *end_pos;	// current segment's end position
    hal_float_t *end_vel;	// current segment's end velocity
    hal_float_t *end_acc;	// current segment's end acceleration

    hal_float_t *curr_pos;	// current position
    hal_float_t *curr_vel;	// current velocity
    hal_float_t *curr_acc;	// current acceleration

	hal_bit_t   *traj_busy;      // no pending trajectory

    double       coeff[6];      // of current segment
};

struct inst_data {
    int count;
    ringbuffer_t traj;            // trajectory segments ringbuffer

    hal_bit_t   *jitter_correct;  // use actual instead of assumed thread time
    hal_float_t *epsilon;         // limit below we switch to linear interpolation

    hal_u32_t   *degree;          // degree of interpolation, 1:linear 3:cubic 5:quintic
    hal_s32_t   *serial;          // current segment serial, -1 = None
    hal_float_t *duration;        // current segment duration
    hal_float_t *progress;        // time spent on current segment so far
    double       time_from_start; // current segment starting time

    double       powers[6];       // once per segment - for all joints
    double       pnow[6];         // once per segment - for all joints, depends on invocation time
    struct joint joints[0];
};

static inline bool segment_completed(const struct inst_data *ip, const  double period)
{
    // consider a segment completed if duration - progress < invocation time:
    return (*(ip->duration) - *(ip->progress) - period) < 0.0;
}

static inline void generatePowers(int n, double x, double* powers)
{
    int i;
    powers[0] = 1.0;
    for (i = 1; i <= n; i++)
		powers[i] = powers[i-1]*x;
}

static int interpolate_joint(struct inst_data *ip,
			     struct joint *jp,
			     const double duration, const bool linear)
{
    double pos = 0.0;
    double vel = 0.0;
    double acc = 0.0;
    switch (*(ip->degree)) {
    case 5:
		pos += ip->pnow[4] * jp->coeff[4];
		pos += ip->pnow[5] * jp->coeff[5];
		vel += 4.0 * ip->pnow[3] * jp->coeff[4] +
		    5.0 * ip->pnow[4] * jp->coeff[5];

		acc += 12.0 * ip->pnow[2] * jp->coeff[4] +
		    20.0 * ip->pnow[3] * jp->coeff[5];
    case 3:
		pos += ip->pnow[2] * jp->coeff[2];
		pos += ip->pnow[3] * jp->coeff[3];

		vel += 2.0 * ip->pnow[1] * jp->coeff[2] +
		    3.0 * ip->pnow[2] * jp->coeff[3];

		acc += 2.0 * ip->pnow[0] * jp->coeff[2] +
		    6.0 * ip->pnow[1] * jp->coeff[3];
    case 1:
		pos += ip->pnow[0] * jp->coeff[0];
		pos += ip->pnow[1] * jp->coeff[1];

		vel += ip->pnow[0] * jp->coeff[1];
	    }
	    *(jp->curr_pos) = pos;
	    *(jp->curr_vel) = vel;
	    *(jp->curr_acc) = acc;
    return 0;
}

// thread function, per-instance
// interpolates all joints of this instance
static int update(void *arg, const hal_funct_args_t *fa)
{
    struct inst_data *ip = (struct inst_data *) arg;
    double period = ((double) fa_period(fa)) * 1e-9;

    int i;
    if (segment_completed(ip, period)) {
		// check for a new JointTrajectoryPoint
		void *data;
		size_t size;
		if (record_read(&ip->traj, (const void**)&data, &size) == 0) {

		    // protobuf-decode it
		    pb_istream_t stream = pb_istream_from_buffer(data, size);
		    pb_JointTrajectoryPoint rx =  pb_JointTrajectoryPoint_init_zero;
		    if (!pb_decode(&stream, pb_JointTrajectoryPoint_fields, &rx)) {
				rtapi_print_msg(RTAPI_MSG_ERR, "%s: pb_decode(JointTrajectoryPoint) failed: '%s'",
					compname, PB_GET_ERROR(&stream));
		    } else {
			// decode ok - start a new segment
				double duration = *(ip->duration) = rx.time_from_start - ip->time_from_start;
	            // the very first point in the ringbuffer is not a segment.
	            // therefore we need to "jump" to these initial settings for the
	            // interpolator to calculate the correct path.
	            // for example, a path can start at position, velocity and acceleration
	            // who are non-zero. In a typical ROS message the first point has a
	            // duration of "0.0"
	            if (duration == 0.0) {
	                // set the start positions
	                // or try out to drop this point later on
	                for (i = 0; i < ip->count; i++) {
	                    struct joint *jp = &ip->joints[i];
						*(jp->traj_busy) = true;
	                    *(jp->curr_pos) = *(jp->end_pos) = rx.positions[i];
	                    *(jp->curr_vel) = *(jp->end_vel) = rx.velocities[i];
	                    *(jp->curr_acc) = *(jp->end_acc) = rx.accelerations[i];
	                    jp->coeff[0] = *(jp->end_pos);
	                    jp->coeff[1] = 0.0;
	                    jp->coeff[2] = 0.0;
	                    jp->coeff[3] = 0.0;
	                    jp->coeff[4] = 0.0;
	                    jp->coeff[5] = 0.0;
	                }
	                // so when we have read the first point, we need to discard everythin
	                // else and make sure we will read the second point, as to complete the
	                // first segment
	            } else {
				    generatePowers(*(ip->degree), duration, ip->powers);
	                ip->time_from_start =  rx.time_from_start;
	                *(ip->progress) = 0.0;
	                for (i = 0; i < rx.positions_count; i++) {
			            struct joint *jp = &ip->joints[i];
						*(jp->traj_busy) = true;
			            double pos2 = *(jp->end_pos) = rx.positions[i];
			    		double vel2 = *(jp->end_vel) = rx.velocities[i];
			    		double acc2 = *(jp->end_acc) = rx.accelerations[i];
	                    double pos1 = *(jp->curr_pos);
	                    double vel1 = *(jp->curr_vel);
	                    double acc1 = *(jp->curr_acc);
					    switch (*(ip->degree)) {
					    case 1:
							jp->coeff[0] = pos1;
							jp->coeff[1] = (pos2 - pos1) / duration;
							jp->coeff[2] = 0.0;
							jp->coeff[3] = 0.0;
							jp->coeff[4] = 0.0;
							jp->coeff[5] = 0.0;
						break;
					    case 3:
							jp->coeff[0] = pos1;
							jp->coeff[1] = vel1;
							jp->coeff[2] = (-3.0*pos1 + 3.0*pos2 - 2.0*vel1*ip->powers[1] - vel2*ip->powers[1]) / ip->powers[2];
							jp->coeff[3] = (2.0*pos1 - 2.0*pos2 + vel1*ip->powers[1] + vel2*ip->powers[1]) / ip->powers[3];
							jp->coeff[4] = 0.0;
							jp->coeff[5] = 0.0;
						break;
					    case 5:
							jp->coeff[0] = pos1;
							jp->coeff[1] = vel1;
							jp->coeff[2] = 0.5 * acc1;
							jp->coeff[3] =  (-20.0*pos1 + 20.0*pos2 - 3.0*acc1*ip->powers[2] + acc2*ip->powers[2] -
									 12.0*vel1*ip->powers[1] - 8.0*vel2*ip->powers[1]) / (2.0*ip->powers[3]);
							jp->coeff[4] =  (30.0*pos1 - 30.0*pos2 + 3.0*acc1*ip->powers[2] - 2.0*acc2*ip->powers[2] +
									 16.0*vel1*ip->powers[1] + 14.0*vel2*ip->powers[1]) / (2.0*ip->powers[4]);
							jp->coeff[5] =  (-12.0*pos1 + 12.0*pos2 - acc1*ip->powers[2] + acc2*ip->powers[2] -
									 6.0*vel1*ip->powers[1] - 6.0*vel2*ip->powers[1]) / (2.0*ip->powers[5]);
						break;
					    }
					}
	        	}
		    }
		    record_shift(&ip->traj);   // consume record
		} else {
	        // segment completed and no new point in ringbuffer
	        for (i = 0; i < ip->count; i++) {
	            struct joint *jp = &ip->joints[i];
				*(jp->traj_busy) = false;
	            jp->coeff[0] = *(jp->end_pos);
	            jp->coeff[1] = 0.0;
	            jp->coeff[2] = 0.0;
	            jp->coeff[3] = 0.0;
	            jp->coeff[4] = 0.0;
	            jp->coeff[5] = 0.0;
	        }
	    }
    }

    *(ip->progress) += period;

    generatePowers(*(ip->degree), *(ip->progress), ip->pnow);
    for (i = 0; i < ip->count; i++) {
		struct joint *jp = &ip->joints[i];
		interpolate_joint(ip, jp, *(ip->progress), 0);
    }
    return 0;
}


static int instantiate_interpolate(const int argc,
				   const char**argv)
{
  const char *name = argv[1];
    struct inst_data *ip;
    int inst_id, i;

    if ((inst_id = hal_inst_create(name, comp_id,
				   sizeof(struct inst_data) +
				   count * sizeof(struct joint),
				   (void **)&ip)) < 0)
	return -1;

    // instance-level values
    ip->count = count;

    // attach the command ringbuffer '<instancename>.traj' if it exists
    // must be record mode
    unsigned flags;
    if (!hal_ring_attachf(&(ip->traj), &flags,  "%s.traj", name)) {
		if ((flags & RINGTYPE_MASK) != RINGTYPE_RECORD) {
		    HALERR("ring %s.traj not a record mode ring: mode=%d",name, flags & RINGTYPE_MASK);
		    return -EINVAL;
		}
		ip->traj.header->reader = inst_id; // we're the reader - advisory
    } else {
		HALERR("ring %s.traj does not exist", name);
		return -EINVAL;
    }

    // per-instance objects
    if (hal_pin_s32_newf(HAL_OUT, &(ip->serial), inst_id, "%s.serial", name) ||
		hal_pin_u32_newf(HAL_IN, &(ip->degree), inst_id, "%s.degree", name) ||
		hal_pin_bit_newf(HAL_IN, &(ip->jitter_correct), inst_id, "%s.jitter-correct", name) ||
		hal_pin_float_newf(HAL_IN, &(ip->epsilon), inst_id, "%s.epsilon", name) ||
		hal_pin_float_newf(HAL_OUT, &(ip->duration), inst_id, "%s.duration", name) ||
		hal_pin_float_newf(HAL_OUT, &(ip->progress), inst_id, "%s.progress", name))
		return -1;
    *(ip->serial) = 0;
    *(ip->degree) = 1;
    *(ip->jitter_correct) = 0;
    *(ip->epsilon) = 0;
    *(ip->duration) = 0;
    *(ip->progress) = 0;
    ip->time_from_start = 0;
    // per-joint objects
    for (i = 0; i < ip->count; i++) {
		struct joint *jp = &ip->joints[i];
		if (hal_pin_float_newf(HAL_OUT, &(jp->end_pos), inst_id, "%s.%d.end-pos", name, i) ||
		    hal_pin_float_newf(HAL_OUT, &(jp->end_vel), inst_id, "%s.%d.end-vel", name, i) ||
		    hal_pin_float_newf(HAL_OUT, &(jp->end_acc), inst_id, "%s.%d.end-acc", name, i) ||
		    hal_pin_float_newf(HAL_OUT, &(jp->curr_pos), inst_id, "%s.%d.curr-pos", name, i) ||
		    hal_pin_float_newf(HAL_OUT, &(jp->curr_vel), inst_id, "%s.%d.curr-vel", name, i) ||
		    hal_pin_float_newf(HAL_OUT, &(jp->curr_acc), inst_id, "%s.%d.curr-acc", name, i) ||
			hal_pin_bit_newf(HAL_OUT, &(jp->traj_busy), inst_id, "%s.%d.traj-busy", name, i))
		    return -1;
        // set all pin values to zero
        *(jp->end_pos) = 0;
        *(jp->end_vel) = 0;
        *(jp->end_acc) = 0;
        *(jp->curr_pos) = 0;
        *(jp->curr_vel) = 0;
        *(jp->curr_acc) = 0;
		*(jp->traj_busy) = false;
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

static int delete_interpolate(const char *name, void *inst, const int inst_size)
{

    struct inst_data *ip = (struct inst_data *) inst;
    int retval;

    if (ringbuffer_attached(&ip->traj)) {
	if ((retval = hal_ring_detachf(&ip->traj, "%s.traj", name)) < 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: hal_ring_detach(%s.traj) failed: %d\n",
			    compname, name, retval);
	    return retval;
	}
	ip->traj.header->reader = 0;
    }
    return 0;
}

int rtapi_app_main(void)
{
    comp_id = hal_xinit(TYPE_RT, 0, 0,
			(hal_constructor_t)instantiate_interpolate,
			delete_interpolate,
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
