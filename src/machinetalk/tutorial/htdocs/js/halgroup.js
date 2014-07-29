
Halgroup.prototype.log = function(message) {
    console.log(message);
}

Halgroup.prototype.define_signal = function(s) {
    this.log("define signal: " + s.name + " type: " + s.type + " value: " + halvalue(s));
}

Halgroup.prototype.update_signal = function(s) {
    this.log("update signal: " + s.name + " type: " + s.type + " value: " + halvalue(s));
}

Halgroup.prototype.closed = function(evt) {
    // var code = evt.code;
    // var reason = evt.reason;
    // var wasClean = evt.wasClean;
    this.halgroup.log("halgroup closed called");
}

Halgroup.prototype.error = function(evt) {
    this.log("halgroup  onerror");
}

Halgroup.prototype.opened = function(evt) {
    this.log("halgroup onopen");
}

function Halgroup() {

    if (! ("WebSocket" in window)) {
	alert("this browser does not support Websockets!");
    }
    // diagnostic counters
    this.errors = 0;
    this.undef = 0;
    this.nogroup = 0;
    this.fullupdates = 0;
    this.incrupdates = 0;

    // signal objects, indexed by handle
    this.signals_by_handle = new Array;
    this.readyCallback = null;

    this.full_update = function(msg) {
	for (var i = 0; i < msg.group.length; ++i) {
            var g = msg.group[i];
            for (var j = 0; j < g.member.length; ++j) {
		var s = g.member[j].signal;
		this.signals_by_handle[s.handle] = s;
		if (typeof this.define_signal == "function")
		    this.define_signal(s);
	    }
	}
    }
    this.incr_update = function(msg) {
	// an update message might have several signals updated by handle
	for (var j = 0; j < msg.signal.length; ++j) {
            var u = msg.signal[j];
            var s = this.signals_by_handle[u.handle];
            if (s == undefined) {
		throw "no signal for handle " + u.handle;
            }
            // update value in signal by copying over all attributes
            // this also copies the handle unnecessarily but who cares
            for (var attrname in u) {
		s[attrname] = u[attrname];
            }
	    if (typeof this.update_signal == "function")
		this.update_signal(s);
	}
    }
}

Halgroup.prototype.connect = function() {
    var uri =  "/?connect=machinekit://halgroup&type=sub&policy=json&text";

    this.log("halgroup connect: " + this.host + ":" + this.port + uri );
    this.socket = new WebSocket('ws://' + this.host + ":" + this.port + uri , "http");
    this.socket.halgroup = this;

    this.socket.onerror   = function (evt) {
	this.halgroup.error(evt);
    }
    this.socket.onclose   = function (evt) {
	this.halgroup.closed(evt);
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
	for (var i = 0; i < this.halgroup.groups.length; i++) {
	    this.halgroup.subscribe(this.halgroup.groups[i]);
	}
	this.halgroup.opened(evt);
    }
    this.socket.onmessage = function(evt) {
	var msg = JSON.parse(evt.data);
	switch (msg.type) {
	case MT_HALGROUP_FULL_UPDATE:
	    this.halgroup.fullupdates += 1;
            this.halgroup.full_update(msg);
            break;
	case MT_HALGROUP_INCREMENTAL_UPDATE:
	    this.halgroup.incrupdates += 1;
            this.halgroup.incr_update(msg);
            break;
	case MT_STP_NOGROUP:
	    this.halgroup.nogroup += 1;
            alert("MT_STP_NOGROUP: \n" + msg.note);
            break;
	case MT_PING:
            // for now, ignore
            // TBD: add a timer, reset on any message received
            // if timeout, emit "rcomp channel down" event/callback
	    break;
	default:
	    this.halgroup.undef += 1;
	    alert("halgroup  onmessage: undefined message type: " + msg.type + "," + evt.data);
	}
    }
}

Halgroup.prototype.ready = function (cb) {
    this.readyCallback = cb;
}

Halgroup.prototype.run = function() {

    // a single group may be given in the URI:
    // http://127.0.0.1:8080/rcomp.html?host=1.2.3.4&port=4711&group=dro
    this.host = $.getUrlVar('host') || location.hostname ;
    this.port = $.getUrlVar('port') || location.port;

    this.groups = [];
    var urigroup = $.getUrlVar('group') || undefined;
    if (typeof urigroup != 'undefined')
	this.groups.push(urigroup);

    // additional group names may be given to run()
    for (var i = 0; i < arguments.length; i++) {
	this.groups.push(arguments[i]);
    }
    if (typeof this.readyCallback == "function")
	this.readyCallback();
    this.connect();
}
