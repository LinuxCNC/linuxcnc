// this component brings a multi-purpose delay line.
//
// input pins of bit, float, u32 or s32 will be measured and put in a buffer
// with a timestamp (hal_delayline_t->input_ts).
// The reading part of this component will look at the record in
// the buffer, and when the timestamp matches it's own timestamp
// (hal_delayline_t->output_ts which is less then hal_delayline_t->input_ts)
// the value will be put into the output pins.
//
// changing the hal_delayline_t->delay pin will set the amount of delay
//
// see manpage delayline.9

#include "rtapi.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "hal.h"
#include "hal_ring.h"
#include "hal_priv.h"

MODULE_AUTHOR("Bas de Bruijn");
MODULE_DESCRIPTION("Delay line component for Machinekit HAL");
MODULE_LICENSE("GPLv2");

#define MAX_INST 8
#define DEFAULT_SAMPLES 1
#define MAX_SAMPLES 9       // a pose has 9 doubles
#define MAX_DELAY 1000      // 1 sec at default servo rate
#define RB_HEADROOM 1.2     // ringbuffer size overallocation

static char *names[MAX_INST] = { "delayline.0", };
RTAPI_MP_ARRAY_STRING(names, MAX_INST,"delayline names");

static int max_delay[MAX_INST] = {
    MAX_DELAY,MAX_DELAY,MAX_DELAY,MAX_DELAY,
    MAX_DELAY,MAX_DELAY,MAX_DELAY,MAX_DELAY
};
RTAPI_MP_ARRAY_INT(max_delay, MAX_INST,"delayline max number of samples");


char *samples[MAX_INST];
RTAPI_MP_ARRAY_STRING(samples, MAX_INST, "delayline pintype sample specifiers");

typedef struct {
    // pins
    hal_bit_t		*enable;			// pin: enable this
    hal_bit_t		*abort;				// pin: abort pin
    hal_data_u		*pins_in[MAX_SAMPLES];		// pin: array incoming value
    hal_data_u		*pins_out[MAX_SAMPLES];		// pin: array delayed value
    hal_u32_t		*pin_delay;			// pin: delay time, standard zero
    hal_u32_t 		*write_fail;			// error counter, write side
    hal_u32_t 		*read_fail;			// error counter, read side
    hal_u32_t 		*too_old;			// error counter, samples skipped
                                                        // because too old

    // other params & instance data
    hal_type_t pintype[MAX_SAMPLES];    // the array which holds the pintype
    __u64  output_ts, input_ts;	// output timestamp and input timestamp
    unsigned  max_delay;         	// max delay of this line in periods
    unsigned  delay;             	// delay in periods
    int nsamples;                	// number of pins in this line
    size_t sample_size;          	// size determined by hal_data_u
    hal_bit_t last_abort;        	// tracking value for edge detection
    char name[HAL_NAME_LEN + 1];	// of this instance
} hal_delayline_t;

typedef struct {
    __u64 timestamp;			// timestamp of measurement
    hal_data_u value[0];		// measured value
} sample_t;


static ringbuffer_t *instance[MAX_INST]; // instance data
static int count;                        // number of instances
static int comp_id;
const char *cname = "delayline";

// thread functions
static void write_sample_to_ring(void *arg, long period);
static void read_sample_from_ring(void *arg, long period);

// this is the routine where the pins are made
static int export_delayline(int n);
static int return_instance_samples(int n);

int rtapi_app_main(void)
{
    int n, retval;

    // determine number of instances
    for (count = 0; (names[count] != NULL) && (count < MAX_INST); count++);

    comp_id = hal_init(cname);
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_init() failed, rc=%d\n", cname, comp_id);
	return -1;
    }

    // check if nr of array members in sample lines configuration string
    // correspond with number of instances. initiation is like:
    // samples="bb,sf,fus" (bit, float, signed, unsigned)
    if (!samples[0]) {
        hal_print_msg(RTAPI_MSG_ERR,
			"%s : ERROR: a string declaring valid pintype is needed\n",
			cname);
        hal_exit(comp_id);
        return -1;
    }

    // parse the samples array for sanity
    int nr_of_samples;
    for (n = 0; (samples[n] != NULL) || (n < count); n++) {
	nr_of_samples = return_instance_samples(n);
	if (nr_of_samples <= 0) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: erroneous number of samples (%d) for instance %d\n",
			    cname, nr_of_samples, n);
	    hal_exit(comp_id);
	    return -1;
	}
	rtapi_print_msg(RTAPI_MSG_DBG,
			"%s: there are %d samples for instance %d\n",
			cname, nr_of_samples, n);
    }
    if (n != count) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "%s: ERROR: \"samples=\" array amount mismatch with number of"
		      " instances\n",	cname);
	hal_exit(comp_id);
	return -1;
    }

    // done with input checking, and all should be fine
    // export variables and function for each DELAYLINE loop
    for (n = 0; n < count; n++) {
	if ((retval = export_delayline(n))) {
	    hal_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: loop %d var export failed\n", cname, n);
	    hal_exit(comp_id);
	    return -1;
	}
    }

    rtapi_print_msg(RTAPI_MSG_DBG,
		    "%s: installed %d lines\n", cname, count);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);

    // detach from all rings, and delete them
    int n;
    for (n = 0; n < count; n++) {
	if (instance[n]) {
	    hal_delayline_t *hd = instance[n]->scratchpad;
	    hal_ring_detachf(instance[n], "%s.samples", hd->name);
	    hal_ring_deletef("%s.samples", hd->name);
	}
    }
}


// write_sample_to_ring() will sample the pins and add them as a struct sample_t
// into the ring.
static void write_sample_to_ring(void *arg, long period)
{
    int j;
    ringbuffer_t *rb = (ringbuffer_t *) arg;
    hal_delayline_t *hd = rb->scratchpad;    // per-instance HAL data
    sample_t *s;

    // if pin_delay > max_delay then use max_delay, otherwise
    // use delay pin value

    // if time has increased, then take action by setting the input_ts to the
    // result of output_ts + delay. Otherwise do nothing. The situation of
    // decreasing the time will be acted upon in read_sample_to_ring()
    if ( *(hd->pin_delay) > (hal_u32_t)( hd->input_ts - hd->output_ts)) {
	if ( *(hd->pin_delay) > hd->max_delay) {
	    hd->delay = hd->max_delay;
	}
	else {
	    hd->delay = *(hd->pin_delay);
	}
	// set the new timestamp
	hd->input_ts = hd->output_ts + (__u64)(hd->delay);
    }

    if (!*(hd->enable)) {
	// skip if not sampling, just put the input
	// values into the output values
	for (j = 0; j < hd->nsamples; j++) {
	    switch (hd->pintype[j])
		{
		case HAL_BIT:
		    hd->pins_out[j]->b = hd->pins_in[j]->b;
		    break;
		case HAL_FLOAT:
		    hd->pins_out[j]->f = hd->pins_in[j]->f;
		    break;
		case HAL_S32:
		    hd->pins_out[j]->s = hd->pins_in[j]->s;
		    break;
		case HAL_U32:
		    hd->pins_out[j]->u = hd->pins_in[j]->u;
		    break;
		case HAL_TYPE_UNSPECIFIED:
		    // an error - should fail loudly TBD
		    ;
		}
	}
	goto DONE;
    }

    // use non-copying write:
    // allocate space in the ringbuffer and retrieve a pointer to it
    if (record_write_begin(rb, (void ** )&s, hd->sample_size)) {
	*(hd->write_fail) += 1;
	goto DONE;
    }

    // deposit record directly in rb memory
    s->timestamp = hd->input_ts;
    for (j = 0; j < hd->nsamples; j++) {
	switch (hd->pintype[j])
	    {
	    case HAL_BIT:
		s->value[j].b = hd->pins_in[j]->b;
		break;
	    case HAL_FLOAT:
		s->value[j].f = hd->pins_in[j]->f;
		break;
	    case HAL_S32:
		s->value[j].s = hd->pins_in[j]->s;
		break;
	    case HAL_U32:
		s->value[j].u = hd->pins_in[j]->u;
		break;
	    case HAL_TYPE_UNSPECIFIED:
		// an error - should fail loudly TBD
		;
	    }
    }

    // commit the write given the actual write size (which is the same
    // as given in record_write_begin in our case). This makes the
    // record actually visible on the read side (advances pointer)
    if (record_write_end(rb, s, hd->sample_size))
	*(hd->write_fail) += 1;

 DONE:
    hd->input_ts++;
}

// sample the output pins to the current rb record
static inline void apply(const sample_t *s, const hal_delayline_t *hd)
{
    int i;
    for (i = 0; i < hd->nsamples; i++)	{
	// because of the union the values and pins must be properly
	// dereferenced against their type
	switch (hd->pintype[i])  {
	case HAL_BIT:
	    hd->pins_out[i]->b = s->value[i].b;
	    break;
	case HAL_FLOAT:
	    hd->pins_out[i]->f = s->value[i].f;
		break;
	case HAL_S32:
	    hd->pins_out[i]->s = s->value[i].s;
	    break;
	case HAL_U32:
	    hd->pins_out[i]->u = s->value[i].u;
	    break;
	case HAL_TYPE_UNSPECIFIED:
	    // an error - should fail loudly TBD
	    ;
	}
    }
}

static void read_sample_from_ring(void *arg, long period)
{
    ringbuffer_t *rb = (ringbuffer_t *) arg;
    hal_delayline_t *hd = rb->scratchpad;
    const sample_t *s;
    size_t size;

    // detect rising edge on abort pin, and flush rb if so
    if (*(hd->abort) && (*(hd->abort) ^ hd->last_abort)) {
	int dropped = record_flush(rb);
	rtapi_print_msg(RTAPI_MSG_INFO,
			"%s: %s aborted - dropped %d samples\n",
			cname, hd->name, dropped);
    }

    // if pin_delay < 0 then use 0, otherwise
    // use delay pin value

    // if time has decreased, then take action by setting the output_ts to the
    // result of input_ts - delay. Otherwise do nothing. This will result in
    // loss of some records. Make sure you're in a safe situation!
    // The situation of increasing the time will be acted upon in
    // write_sample_to_ring()
    if ( *(hd->pin_delay) < (hal_u32_t)(hd->input_ts - hd->output_ts)) {
	if ( *(hd->pin_delay) < 0) {
	    hd->delay = 0;
	}
	else {
	    hd->delay = *(hd->pin_delay);
	}
	// set the new timestamp
	hd->input_ts =  hd->input_ts - (__u64)(hd->delay);
    }

    // peek at the head of the queue
    while (record_read(rb, (const void **)&s, &size) == 0) {

	// do nothing if timestamp is in the future
	if (s->timestamp > hd->output_ts)
	    goto NOTYET;

	if (s->timestamp == hd->output_ts)
	    // the time is right
	    apply(s, hd);
	else
	    // skip old samples and bump an error counter
	    *(hd->too_old) += 1;

	// sanity: if (size != hd->sample_size).. terribly wrong.
	record_shift(rb); // consume record
    }
 NOTYET:
    hd->output_ts++; // always bump the timestamp
    hd->last_abort = *(hd->abort);
}

static int return_instance_samples(int n)
{
    int i, nr_of_samples = 0;
    char character;
    // traverse the string to count the number of correct pins
    for (i = 0; (character = samples[n][i]); i++) {
	switch (character) {
	case 'b':
	case 'B':
	case 'f':
	case 'F':
	case 's':
	case 'S':
	case 'u':
	case 'U':
	    nr_of_samples++;
	    break;
	default:
	    rtapi_print_msg(RTAPI_MSG_ERR, "invalid character in "
			    "\"samples=\" string. Needs to be only b,f,s or u\n");
	    hal_exit(comp_id);
	    return -1;
	}
    }
    return nr_of_samples;
}

static int export_delayline(int n)
{
    int retval, nr_of_samples, i;
    char buf[HAL_NAME_LEN + 1], *str_type;

    // determine the required size of the ringbuffer
    nr_of_samples = return_instance_samples(n);
    size_t sample_size = sizeof(sample_t) + (nr_of_samples * sizeof(hal_data_u));

    // add some headroom to be sure we dont overrun
    size_t rbsize = record_space(sample_size) * max_delay[n] * RB_HEADROOM;

    // create the delay queue
    rtapi_snprintf(buf, HAL_NAME_LEN, "%s.samples", names[n]);
    if ((retval = hal_ring_new(buf, rbsize,
			       sizeof(hal_delayline_t), ALLOC_HALMEM))) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "%s: failed to create new ring %s: %d\n",
		      cname, buf, retval);
	return -1;
    }

    // use the per-using component ring access structure as the instance data,
    // which will also give us a handle on the scratchpad which we use for
    // HAL pins and other shared data
    if ((instance[n] = hal_malloc(sizeof(ringbuffer_t))) == NULL)
	return -1;
    if ((retval = hal_ring_attach(buf, instance[n], NULL))) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "%s: attach to ring %s failed: %d\n",
		      cname, buf, retval);
	return -1;
    }

    // fill in instance data
    hal_delayline_t *hd = instance[n]->scratchpad;
    strncpy(hd->name, names[n], sizeof(hd->name));
    hd->nsamples = nr_of_samples;
    hd->sample_size = sample_size;
    hd->max_delay = max_delay[n];
    // set delay standard to value of zero
    hd->delay = 0;
    hd->output_ts = 0;
    hd->input_ts = hd->delay;

    // init pins, and at the same time fill the puntype array with
    // the type of pin[i] so we can later dereference proper
    char character;
    for (i = 0; i < hd->nsamples; i++) {
	character = samples[n][i];
	rtapi_print_msg(RTAPI_MSG_DBG, "\"samples=\" string = %s"
			" character %d = %c \n",
			samples[n], i, character);
	// determine type
	hal_type_t type = HAL_TYPE_UNSPECIFIED;
	switch (character) {
	case 'b':
	case 'B':
	    type = HAL_BIT;
	    break;
	case 'f':
	case 'F':
	    type = HAL_FLOAT;
	    break;
	case 's':
	case 'S':
	    type = HAL_S32;
	    break;
	case 'u':
	case 'U':
	    type = HAL_U32;
	    break;
	default:
	    hal_print_msg(RTAPI_MSG_ERR,
			  "%s: invalid type '%c' for pin %d\n",
			  cname, character, i);
	    return -EINVAL;
	}
	hd->pintype[i] = type;
	switch (type) {
	case HAL_BIT:
	    str_type = "bit";
	    break;
	case HAL_FLOAT:
	    str_type = "flt";
	    break;
	case HAL_U32:
	    str_type = "u32";
	    break;
	case HAL_S32:
	    str_type = "s32";
	    break;
	case HAL_TYPE_UNSPECIFIED:
	    // do nothing
	    break;
	}
	// create pins of type as requested
	if (((retval = hal_pin_newf(type,
				    HAL_IN,
				    (void **) &hd->pins_in[i],
				    comp_id,
				    "%s.in%d-%s",
				    hd->name, i, str_type)) < 0) ||
	    ((retval = hal_pin_newf(type,
				    HAL_OUT,
				    (void **) &hd->pins_out[i],
				    comp_id,
				    "%s.out%d-%s",
				    hd->name, i, str_type)) < 0)) {
	    return retval;
	}
	// post hal_pin_new(), pins are guaranteed to be set to zero
    }

    // create other pins
    if (((retval = hal_pin_bit_newf(HAL_IN, &(hd->enable), comp_id,
				    "%s.enable",  hd->name))) ||
	((retval = hal_pin_bit_newf(HAL_IN, &(hd->abort), comp_id,
				    "%s.abort",  hd->name))) ||
	((retval = hal_pin_u32_newf(HAL_IN, &(hd->pin_delay), comp_id,
				    "%s.delay", hd->name))) ||
	((retval = hal_pin_u32_newf(HAL_OUT, &(hd->write_fail), comp_id,
				    "%s.write-fail", hd->name))) ||
	((retval = hal_pin_u32_newf(HAL_OUT, &(hd->read_fail), comp_id,
				    "%s.too-old", hd->name))) ||
	((retval = hal_pin_u32_newf(HAL_OUT, &(hd->too_old), comp_id,
				    "%s.read-fail", hd->name))))
	return retval;

    // export thread functions
    if (((retval = hal_export_functf(write_sample_to_ring,
				    instance[n], 1, 0, comp_id,
				     "%s.sample", hd->name)) < 0) ||

	((retval = hal_export_functf(read_sample_from_ring,
				     instance[n], 1, 0, comp_id,
				     "%s.output", hd->name)) < 0)) {
	return retval;
    }
    return 0;
}
