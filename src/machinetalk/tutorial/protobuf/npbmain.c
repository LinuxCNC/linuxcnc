#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <pb_decode.h>
#include <demo.npb.h>

static void ptarget(const char *tag, const pb_Target *t)
{
    if (t->has_x) { printf("%sx=%f\n", tag, t->x); }
    if (t->has_y) { printf("%sy=%f\n", tag, t->y); }
    if (t->has_z) { printf("%sz=%f\n", tag, t->z); }
}

void process(const pb_DemoContainer *d)
{
    int i;

    printf("Container type=%d\n", d->type);
    if (d->has_velocity) printf("default velocity %f\n", d->velocity);
    if (d->has_acceleration) printf("default acceleration %f\n", d->acceleration);

    for (i = 0; i < d->segment_count; i++) {
	const pb_Segment *s = &d->segment[i];

	printf("\tSegment type=%d\n", s->type);
	if (s->has_end) ptarget("\t\tend.", &s->end);
	if (s->has_normal) ptarget("\t\tnormal.", &s->normal);
	if (s->has_center) ptarget("\t\tcenter.", &s->center);
    }
}


// nanopb reader callback, see http://koti.kapsi.fi/~jpa/nanopb/docs/
bool callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    FILE *file = (FILE*)stream->state;
    bool status;
    status = (fread(buf, 1, count, file) == count);
    if (feof(file))
        stream->bytes_left = 0;
    return status;
}

int main()
{
    pb_istream_t stream = { &callback, stdin, 10000};
    pb_DemoContainer d = { 0 };

    if (!pb_decode(&stream, pb_DemoContainer_fields, &d)) {
	fprintf(stderr,"pb_decode(DemoContainer) failed: '%s'\n", PB_GET_ERROR(&stream));
	exit(1);
    }
    process(&d);
    exit(0);
}
