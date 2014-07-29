var  MT_LOG_MESSAGE    = 10;
var MT_ZMQ_SUBSCRIBE      = 150;
var MT_ZMQ_UNSUBSCRIBE   = 151;

Logger.prototype.log = function(message) {
    console.log(message);
}

Logger.prototype.define_signal = function(s) {
    this.log("define signal: " + s.name + " type: " + s.type + " value: " + halvalue(s));
}

Logger.prototype.log_message = function(msg) {
    this.log("logger message: " + JSON.stringify(msg));
}

Logger.prototype.closed = function(evt) {
    this.log("logger closed");
}

Logger.prototype.error = function(evt) {
    this.log("Logger onerror");
}

Logger.prototype.opened = function(evt) {
    this.log("Logger onopen");
}

function Logger() {
    if (! ("WebSocket" in window)) {
	alert("this browser does not support Websockets!");
    }
    this.readyCallback = null;
}

Logger.prototype.run = function() {

    this.host = $.getUrlVar('host') || location.hostname ;
    this.port = $.getUrlVar('port') || location.port;

    if (typeof this.readyCallback == "function")
	this.readyCallback();

    var uri =  "/?connect=machinekit://log&type=sub&subscribe=log&policy=json&text";

    this.log("Logger connect: " + this.host + ":" + this.port + uri );
    this.socket = new WebSocket('ws://' + this.host + ":" + this.port + uri , "http");
    this.socket.logger = this;

    this.socket.onerror   = function (evt) {
	this.logger.error(evt);
    }
    this.socket.onclose   = function (evt) {
	this.logger.closed(evt);
    }
    this.socket.onopen    = function (evt) {
	this.logger.opened(evt);
    }
    this.socket.onmessage = function(evt) {
	var msg = JSON.parse(evt.data);
	switch (msg.type) {
	case  MT_LOG_MESSAGE:
            this.logger.log_message(msg);
            break;
	default:
	    alert("logger  onmessage: undefined message type: " + msg.type + "," + evt.data);
	}
    }
}

Logger.prototype.ready = function (cb) {
    this.readyCallback = cb;
}
