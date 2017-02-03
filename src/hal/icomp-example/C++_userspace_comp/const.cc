/**********************************************************************
 * Demo of writing a standard userspace component in std C++
 *
 * Based on the constant.icomp component
 *
 * ArcEye 2015  <arceyeATmgwareDOTcoDOTuk>
 *
***********************************************************************/

#include <LCNC_Headers.h>

using namespace std;

struct __comp_state
    {
    hal_float_t *out;
    hal_float_t *value;
    };


class Const
{
public:
    Const(int argc, char** argv);
    ~Const();

private:
    int connectPins();
    void eventsLoop();

    int comp_id;
    char prefix[12];
    struct __comp_state *inst;
};


Const::Const(int argc, char** argv)
{
int r;

    strcpy(prefix, "const\0");  // used later in pins
    comp_id = hal_init("const");

    if(comp_id < 0)
        exit(-1);

    if((r = connectPins()))
        hal_exit(comp_id);
    else
        {
        hal_ready(comp_id);
        cout << "Component \""<< prefix << "\" registered and ready\n"
            << "comp_ID = " << comp_id << "\n" << endl;
        }

    eventsLoop();
}


Const::~Const()
{
    hal_exit(comp_id);
}


int Const::connectPins()
{
int r = 0;
int sz = sizeof(struct __comp_state) + 2;  //fudge factor

    inst = (struct __comp_state*)hal_malloc(sz);
    memset(inst, 0, sz);

    if( (r = hal_pin_float_newf(HAL_OUT, &(inst->out), comp_id,"%s.out", prefix)) != 0)
        return -1;

    *(inst->out) = 0.0;

    if( (r = hal_pin_float_newf(HAL_IN, &(inst->value), comp_id,"%s.value", prefix)) != 0)
        return -1;

    *(inst->value) = 0.0;

    return 0;
}


void Const::eventsLoop()
{
    while(1)
        {
        *(inst->out) = *(inst->value);
        usleep(500000);
        }
}

int main(int argc, char *argv[])
{
    Const c(argc, argv);

    return 0;
}
