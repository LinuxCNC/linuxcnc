var TYPE_REMOTE = 4;

var MT_PING            = 210;  // keepalive test
var MT_PING_ACKNOWLEDGE = 215;
var MT_STP_NOGROUP = 27;

// in-band zeroMQ commands, evaluated in webtalk_jsonpolicy.cc

var MT_ZMQ_SUBSCRIBE      = 150;
var MT_ZMQ_UNSUBSCRIBE   = 151;

// rcomp creation and binding
var MT_HALRCOMP_BIND  = 256;
var MT_HALRCOMP_BIND_CONFIRM  = 257;
var MT_HALRCOMP_BIND_REJECT  = 258;

// the following to a similar to var MT_HALRCOMMAND_SET and  var MT_HALRCOMMAND_GET, except
// in the context of HALRcomp, only pins and params are evaluated
// also, to set a pin or param, it must have direction HAL_OUT or HAL_IO
var MT_HALRCOMP_SET  = 259;
var MT_HALRCOMP_SET_REJECT  = 260;
var MT_HALRCOMP_ACK  = 263;

// HAL object general setter/getter operations
// these work as viewed from halcmd and include signals
var MT_HALRCOMMAND_SET = 265;
var MT_HALRCOMMAND_SET_REJECT = 266;

var MT_HALRCOMMAND_GET = 267;
var MT_HALRCOMMAND_GET_REJECT = 268;

var MT_HALRCOMMAND_CREATE = 269;
var MT_HALRCOMMAND_CREATE_REJECT = 270;

var MT_HALRCOMMAND_DELETE = 271;
var MT_HALRCOMMAND_DELETE_REJECT = 272;

var MT_HALRCOMMAND_ACK = 273;
var MT_HALRCOMMAND_ERROR = 274;

// introspection
var MT_HALRCOMMAND_DESCRIBE  = 276;
var MT_HALRCOMMAND_DESCRIPTION  = 277;

// rcomp tracking
var MT_HALRCOMP_FULL_UPDATE = 288;
var MT_HALRCOMP_INCREMENTAL_UPDATE = 289;
var MT_HALRCOMP_ERROR = 290;

// group creation and binding
var MT_HALGROUP_BIND  = 294;
var MT_HALGROUP_BIND_CONFIRM  = 295;
var MT_HALGROUP_BIND_REJECT  = 296;

// group tracking
var MT_HALGROUP_FULL_UPDATE = 297;
var MT_HALGROUP_INCREMENTAL_UPDATE = 298;
var MT_HALGROUP_ERROR = 299;

// types
var HAL_BIT = 1;
var HAL_FLOAT = 2;
var HAL_S32 = 3;
var HAL_U32 = 4;

// pin direction
var HAL_IN = 16;
var HAL_OUT = 32;
var HAL_IO = 48;


function halvalue(p) {
    if (p.hasOwnProperty('halbit')) {
        return p.halbit;
    }
    if (p.hasOwnProperty('halfloat')) {
        return p.halfloat;
    }
    if (p.hasOwnProperty('hals32')) {
        return p.hals32;
    }
    if (p.hasOwnProperty('halu32')) {
        return p.halu32;
    }
    throw "halvalue: no value attribute";
}
