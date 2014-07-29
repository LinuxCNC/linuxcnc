"use strict";

var ping_timer = 30000;

Halrcmd.prototype.opened = function(evt) {
    console.log("halrcmd socket opened");
}
Halrcmd.prototype.closed = function(evt) {
     console.log("halrcmd socket closed");
}
Halrcmd.prototype.error = function(msg) {
    console.log("halrcmd error: " + error);
}
Halrcmd.prototype.receive = function(msg) {
    console.log("halrcmd receive: " + JSON.stringify(msg));
}
Halrcmd.prototype.active = function(status) {
    console.log("halrcmd: active=" + status);
}

function Halrcmd(uri) {
    if (! ("WebSocket" in window)) {
	alert("this browser does not support Websockets!");
    }

    this.timer = $.timer(function () {
	if (this.rcmd.ping_outstanding) {
	    this.rcmd.active(false);
	}
	this.rcmd.socket.send(JSON.stringify({type: MT_PING}));
	this.rcmd.ping_outstanding = true;
    });
    this.timer.set({time: ping_timer, autostart: false});
    this.timer.rcmd = this;

    if (uri == undefined) {
	// http://127.0.0.1:8080/rcomp.html?host=1.2.3.4&port=4711
	var host = $.getUrlVar('host') || location.hostname ;
	var port = $.getUrlVar('port') || location.port;
	this.uri = 'ws://' + host + ":" + port + "/?connect=machinekit://halrcmd&type=dealer&policy=json&text";
    } else {
	this.uri = uri;
    }
    this.socket = new WebSocket(this.uri , "http");
    this.socket.rcmd      = this; // back reference to Halrcmd object
    this.socket.onopen    = function (evt) {
	this.rcmd.timer.play();
	this.rcmd.opened(evt);
    }
    this.socket.onclose   = function(evt) {
	this.rcmd.timer.stop();
	this.rcmd.closed(evt);
    }
    this.socket.onerror   = function(evt) {
	this.rcmd.error(evt.data);
    }
    this.socket.onmessage = function(evt) {
	this.rcmd.ping_outstanding = false;
	this.rcmd.timer.reset();
	var msg = JSON.parse(evt.data);
	if (msg.type != MT_PING_ACKNOWLEDGE) {
	    this.rcmd.receive(msg);
	}
	this.rcmd.active(true);
    };
    this.ping_outstanding = false;
}


Halrcomp.prototype.log = function(message) {
    console.log(message);
}

Halrcomp.prototype.define_pin = function(p) {
    this.log("halrcomp define pin: " + JSON.stringify(p));
}

Halrcomp.prototype.update_pin = function(p) {
    this.log("halrcomp update pin: "  + JSON.stringify(p));
}

Halrcomp.prototype.opened = function(evt) {
    console.log("halrcomp socket opened");
}
Halrcomp.prototype.closed = function(evt) {
     console.log("halrcomp socket closed");
}
Halrcomp.prototype.error = function(msg) {
    console.log("halrcomp error: " + error);
}
Halrcomp.prototype.receive = function(msg) {
    console.log("halrcomp receive: " + JSON.stringify(msg));
}

function Halrcomp(uri) {
    if (! ("WebSocket" in window)) {
	alert("this browser does not support Websockets!");
    }
    // diagnostic counters
    this.errors = 0;
    this.undef = 0;
    this.fullupdates = 0;
    this.incrupdates = 0;
    this.pings = 0;

    // pin objects, indexed by handle
    this.pins_by_handle = new Array;

    this.full_update = function(msg) {
	for (var i = 0; i < msg.comp.length; ++i) {
            var c = msg.comp[i];
            for (var j = 0; j < c.pin.length; ++j) {
		var p = c.pin[j];
		this.pins_by_handle[p.handle] = p;
		if (typeof this.define_pin == "function")
		    this.define_pin(p);
            }
	}
    }
    this.incr_update = function(msg) {
	for (var j = 0; j < msg.pin.length; ++j) {
            var u = msg.pin[j];
            var p = this.pins_by_handle[u.handle];
            if (p == undefined) {
		this.errors += 1;
		this.log("no pin for handle " + u.handle);
            }
            // update value in pin by copying over all attributes
            // this also copies the handle unnecessarily but harmless
            for (var attrname in u) {
		p[attrname] = u[attrname];
            }
	    if (typeof this.update_pin == "function")
		this.update_pin(p);
	}
    }
}

Halrcomp.prototype.connect = function() {
    var uri =  "/?connect=machinekit://halrcomp&type=sub&policy=json&text";

    this.log("halrcomp connect: " + this.host + ":" + this.port + uri );
    this.socket = new WebSocket('ws://' + this.host + ":" + this.port + uri , "http");
    this.socket.halrcomp = this;

    this.socket.onerror   = function (evt) {
	this.halrcomp.error(evt);
    }
    this.socket.onclose   = function (evt) {
	this.halrcomp.closed(evt);
    }
    this.subscribe = function (topic) {
	this.log("halrcomp subscribe: " + topic);
	this.socket.send(JSON.stringify({type: MT_ZMQ_SUBSCRIBE,note:[topic]}));
    }
    this.unsubscribe = function (topic) {
	this.log("halrcomp unsubscribe: " + topic);
	this.socket.send(JSON.stringify({type: MT_ZMQ_UNSUBSCRIBE,note:[topic]}));
    }
    this.socket.onopen    = function (evt) {
	for (var i = 0; i < this.halrcomp.comps.length; i++) {
	    this.halrcomp.subscribe(this.halrcomp.comps[i]);
	}
	this.halrcomp.opened(evt);
    }
    this.socket.onmessage = function(evt) {
	var msg = JSON.parse(evt.data);
	switch (msg.type) {
	case MT_HALRCOMP_FULL_UPDATE:
	    this.halrcomp.fullupdates += 1;
            this.halrcomp.full_update(msg);
            break;
	case MT_HALRCOMP_INCREMENTAL_UPDATE:
	    this.halrcomp.incrupdates += 1;
            this.halrcomp.incr_update(msg);
            break;
	case MT_HALRCOMP_ERROR:
	    this.halrcomp.errors += 1;
            alert("MT_HALRCOMP_ERROR: \n" + msg.note);
            break;
	case MT_PING:
	    this.halrcomp.pings += 1;
            // for now, ignore
            // TBD: add a timer, reset on any message received
            // if timeout, emit "rcomp channel down" event/callback
	    break;
	default:
	    this.halrcomp.undef += 1;
	    alert("halrcomp  onmessage: undefined message type: " + msg.type + "," + evt.data);
	}
    }
}

Halrcomp.prototype.ready = function (cb) {
    this.readyCallback = cb;
}

Halrcomp.prototype.run = function() {

    this.host = $.getUrlVar('host') || location.hostname ;
    this.port = $.getUrlVar('port') || location.port;

    this.comps = [];
    for (var i = 0; i < arguments.length; i++) {
	this.comps.push(arguments[i]);
    }
    if (typeof this.readyCallback == "function")
	this.readyCallback();
    this.connect();
}
