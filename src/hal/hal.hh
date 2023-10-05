#ifndef HALXX_HH
#define HALXX_HH

#include <string>
#include <hal.h>

enum class hal_dir{
    UNSPECIFIED = HAL_DIR_UNSPECIFIED,
    IN = HAL_IN,
    OUT = HAL_OUT,
    IO = (HAL_IN | HAL_OUT),
};

template<typename T, hal_dir D>
class hal_pin{
    public:
    volatile T** ptr;
    T operator=(const T& value){
        **ptr = value;
        return **ptr;
    }
    operator T(){
        return **ptr;
    }
};

class hal_comp{
    int comp_id;
    std::string comp_name;
    template<hal_dir D>
    int add_pin_(hal_pin<bool,D> pin, std::string name){
        return hal_pin_new(name.c_str(), HAL_BIT, static_cast<hal_pin_dir_t>(D), (void **)(pin.ptr), comp_id);
    }
    template<hal_dir D>
    int add_pin_(hal_pin<int32_t,D> pin, std::string name){
        return hal_pin_new(name.c_str(), HAL_S32, static_cast<hal_pin_dir_t>(D), (void **)(pin.ptr), comp_id);
    }
    template<hal_dir D>
    int add_pin_(hal_pin<uint32_t,D> pin, std::string name){
        return hal_pin_new(name.c_str(), HAL_U32, static_cast<hal_pin_dir_t>(D), (void **)(pin.ptr), comp_id);
    }
    template<hal_dir D>
    int add_pin_(hal_pin<double,D> pin, std::string name){
        return hal_pin_new(name.c_str(), HAL_FLOAT, static_cast<hal_pin_dir_t>(D), (void **)(pin.ptr), comp_id);
    }
    public:
    int error = 0;
    hal_comp(std::string name){
        comp_id = hal_init(name.c_str());
        comp_name = name;
        if(comp_id < 0){
            error -= 1;
            rtapi_print_msg(RTAPI_MSG_ERR, "%s ERROR: hal_init() failed\n", comp_name.c_str());
            hal_exit(comp_id);
        }
    }
    hal_comp() = delete;

    template<typename T, hal_dir D>
    void add_pin(hal_pin<T,D> &pin, std::string pin_name){
        pin.ptr = (volatile T**)hal_malloc(8);
        if(!pin.ptr){
            error -= 1;
            rtapi_print_msg(RTAPI_MSG_ERR, "%s ERROR: hal_malloc() failed\n", pin_name.c_str());
            hal_exit(comp_id);
        }
        error += add_pin_(pin,comp_name + "." + pin_name);
        if(error < 0){
            rtapi_print_msg(RTAPI_MSG_ERR, "%s ERROR: hal_pin_new() failed\n", pin_name.c_str());
            hal_exit(comp_id);
        }
    }
    ~hal_comp(){
        hal_ready(comp_id);
    }
};

#endif
