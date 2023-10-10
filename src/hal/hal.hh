#ifndef HALXX_HH
#define HALXX_HH

#include <map>
#include <string>
#include <variant>
#include <hal.h>
#include <hal_priv.h>

enum class hal_dir{
    IN = HAL_IN,
    OUT = HAL_OUT,
};

class hal{
    public:
    static bool component_exists(std::string name){
        return halpr_find_comp_by_name(name.c_str()) != NULL;
    }
    static bool pin_has_writer(std::string name){
        hal_pin_t *pin = halpr_find_pin_by_name(name.c_str());
        if(!pin) {//pin does not exist
            return false;
        }
        if(pin->signal) {
            hal_sig_t *signal = (hal_sig_t*)SHMPTR(pin->signal);
            return signal->writers > 0;
        }
        return false;
    }
    static bool component_is_ready(std::string name){
        // Bad form to assume comp name exists - stop crashing!
        hal_comp_t *thecomp = halpr_find_comp_by_name(name.c_str());
        return thecomp && (thecomp->ready != 0);
    }
};

template<typename T>
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

using pin_t = std::variant<hal_pin<double>,hal_pin<bool>,hal_pin<int32_t>,hal_pin<uint32_t>>;

class hal_comp{
    int comp_id;
    std::string comp_name;
    std::map<std::string,pin_t> map;
    int add_pin_(std::string name, hal_dir dir, hal_pin<bool> pin){
        return hal_pin_new(name.c_str(), HAL_BIT, static_cast<hal_pin_dir_t>(dir), (void **)(pin.ptr), comp_id);
    }
    int add_pin_(std::string name, hal_dir dir, hal_pin<int32_t> pin){
        return hal_pin_new(name.c_str(), HAL_S32, static_cast<hal_pin_dir_t>(dir), (void **)(pin.ptr), comp_id);
    }
    int add_pin_(std::string name, hal_dir dir, hal_pin<uint32_t> pin){
        return hal_pin_new(name.c_str(), HAL_U32, static_cast<hal_pin_dir_t>(dir), (void **)(pin.ptr), comp_id);
    }
    int add_pin_(std::string name, hal_dir dir, hal_pin<double> pin){
        return hal_pin_new(name.c_str(), HAL_FLOAT, static_cast<hal_pin_dir_t>(dir), (void **)(pin.ptr), comp_id);
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

    void newpin(std::string name, hal_type_t type, hal_dir dir){
        auto& pin = map[name];
        switch(type){
            case HAL_BIT:
            pin = hal_pin<bool>();
            add_pin(name, dir, std::get<hal_pin<bool>>(pin));
            break;
            case HAL_FLOAT:
            pin = hal_pin<double>();
            add_pin(name, dir, std::get<hal_pin<double>>(pin));
            break;
            case HAL_S32:
            pin = hal_pin<int32_t>();
            add_pin(name, dir, std::get<hal_pin<int32_t>>(pin));
            break;
            case HAL_U32:
            pin = hal_pin<uint32_t>();
            add_pin(name, dir, std::get<hal_pin<uint32_t>>(pin));
            break;
            [[fallthrough]];
            default:
            break;
        }
    }

    std::variant<double,bool,int32_t,uint32_t> getitem(std::string name){
        auto pin = map[name];
        if (auto* v = std::get_if<hal_pin<double>>(&pin)) {
            return *v;
        } else if (auto* v = std::get_if<hal_pin<bool>>(&pin)) {
            return *v;
        } else if (auto* v = std::get_if<hal_pin<int32_t>>(&pin)) {
            return *v;
        } else if (auto* v = std::get_if<hal_pin<uint32_t>>(&pin)) {
            return *v;
        }
        return 0;
    }

    template<typename T>
    void setitem(std::string name, T value){
        auto pin = map[name];
        if (auto* p = std::get_if<hal_pin<double>>(&pin)) {
            *p = value;
        } else if (auto* p = std::get_if<hal_pin<bool>>(&pin)) {
            *p = value;
        } else if (auto* p = std::get_if<hal_pin<int32_t>>(&pin)) {
            *p = value;
        } else if (auto* p = std::get_if<hal_pin<uint32_t>>(&pin)) {
            *p = value;
        }
    }

    void ready(){
        hal_ready(comp_id);
    }

    template<typename T>
    void add_pin(std::string pin_name, hal_dir dir, hal_pin<T> &pin){
        pin.ptr = (volatile T**)hal_malloc(8);
        if(!pin.ptr){
            error -= 1;
            rtapi_print_msg(RTAPI_MSG_ERR, "%s ERROR: hal_malloc() failed\n", pin_name.c_str());
            hal_exit(comp_id);
        }
        error += add_pin_(comp_name + "." + pin_name, dir, pin);
        if(error < 0){
            rtapi_print_msg(RTAPI_MSG_ERR, "%s ERROR: hal_pin_new() failed\n", pin_name.c_str());
            hal_exit(comp_id);
        }
    }

    ~hal_comp(){
        hal_exit(comp_id);
    }
};

#endif
