
function Connection(path, host, protocol) {
    var self = this;
    this.host = host || location.host;
    this.path = path;
    if(window.WebSocket) {
        if(window.location.protocol == 'http:') {
            this.conn = new WebSocket('ws://' + this.host + path, protocol);
        } else {
            this.conn = new WebSocket('wss://' + this.host + path, protocol);
        }
        this.conn.onerror = function(ev) { self.http_fallback(ev); }
    } else {
        this.conn = new HTTPPolling('http://' + this.host + path);
        this.conn.onerror = function(ev) { self.call_onerror(ev); }
    }
    this.conn.onopen = function(ev) { self.call_onopen(ev); }
    this.conn.onmessage = function(ev) { self.call_onmessage(ev); }
    this.conn.onclose = function(ev) { self.call_onclose(ev); }
    this.readyState = this.CONNECTING;
}

Connection.prototype.CONNECTING = 0;
Connection.prototype.OPEN = 1;
Connection.prototype.CLOSING = 2;
Connection.prototype.CLOSED = 3;

Connection.prototype.http_fallback = function () {
    var self = this;
    this.conn = new HTTPPolling('http://' + this.host + this.path);
    this.conn.onerror = function(ev) { self.call_onerror(ev); }
    this.conn.onopen = function(ev) { self.call_onopen(ev); }
    this.conn.onmessage = function(ev) { self.call_onmessage(ev); }
    this.conn.onclose = function(ev) { self.call_onclose(ev); }
}

Connection.prototype.call_onerror = function (ev) {
    this.readyState = this.conn.readyState;
    var f = this.onerror;
    if(f) {
        return f(ev);
    }
}

Connection.prototype.call_onopen = function (ev) {
    this.readyState = this.OPEN;
    this.conn.onopen = this.call_onopen;
    var f = this.onopen;
    if(f) {
        return f(ev);
    }
}

Connection.prototype.call_onmessage = function (ev) {
    var f = this.onmessage;
    if(f) {
        return f(ev);
    }
}

Connection.prototype.call_onclose = function (ev) {
    this.readyState = this.CLOSED;
    var f = this.onclose;
    if(f) {
        return f(ev);
    }
}

Connection.prototype.close = function (ev) {
    if(this.readyState != this.CLOSED) {
        this.readyState = this.CLOSING;
        this.conn.close();
    }
}

Connection.prototype.send = function (value) {
    if(this.readyState == this.OPEN) {
        this.conn.send(value);
    } else {
        throw Error("Socket is in wrong state");
    }
}


function HTTPPolling(url) {
    var self = this;
    this.url = url;
    this.readyState = this.CONNECTING;
    var req = new XMLHttpRequest();
    req.open('POST', this.url + "?action=CONNECT", true);
    req.setRequestHeader('Content-Type', 'text/plain')
    req.onreadystatechange = function(ev) { self.connect_req(ev); };
    req.send();
    this.nrequests = 1;
}

HTTPPolling.prototype.CONNECTING = 0;
HTTPPolling.prototype.OPEN = 1;
HTTPPolling.prototype.CLOSING = 2;
HTTPPolling.prototype.CLOSED = 3;

HTTPPolling.prototype.connect_req = function(ev) {
    var req = ev.target;
    if(req.readyState != 4) return;
    this.nrequests -= 1;
    if(req.status == 200) {
        this.connection_id = req.responseText;
        this.readyState = this.CONNECTED;
        this.last_message = "";
        if(this.onopen) {
            this.onopen();
        }
        this.request_next();
    } else {
        this.readyState = this.CLOSED;
        this.onerror({
            target: this,
            currentTarget: this,
            http_request: req,
            });
    }
}

HTTPPolling.prototype.request_next = function(data) {
    var self = this;
    var req = new XMLHttpRequest();
    req.open('POST', this.url + "?id=" + this.connection_id
        + "&ack=" + this.last_message + "&rnd=" + Math.random(), true);
    req.setRequestHeader('Content-Type', 'text/plain')
    req.onreadystatechange = function(ev) { self.message_req(ev); };
    req.send(data);
    this.nrequests += 1;
}

HTTPPolling.prototype.message_req = function(ev) {
    var req = ev.target;
    if(req.readyState != 4) return;
    this.nrequests -= 1;
    if(req.status != 200) {
        this.readyState = this.CLOSED;
        this.onerror({
            target: this,
            currentTarget: this,
            http_request: req,
            });
        return;
    }
    if(req.getResponseHeader('X-Messages') == '0') {
        if(!this.nrequests) {
            this.request_next();
        }
        return;
    }
    if(req.getResponseHeader('X-Format') != 'single') {
        throw Error("Unknown format " + req.getResponseHeader('X-Format'));
    }
    var id = req.getResponseHeader('X-Message-ID');
    if(this.last_message != id) {
        this.last_message = id;
        if(!this.nrequests) {
            this.request_next();
        }
        if(this.onmessage) {
            this.onmessage({
                data: req.responseText,
                target: this
                });
        }
    } else {
        if(!this.nrequests) {
            this.request_next();
        }
    }
}

HTTPPolling.prototype.send = function(data) {
    this.request_next(data);
}
