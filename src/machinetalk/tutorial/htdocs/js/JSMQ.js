// Endpoint class

function Endpoint(address) {
    var ClosedState = 0;
    var ConnectingState = 1;
    var ActiveState = 2;

    var reconnectTries = 0;

    console.log("connecting to " + address);
    var webSocket = null;
    var state = ClosedState;

    var that = this;

    var incomingMessage = null;
    var outgoingMessage = null;

    open();

    function open() {
        if (webSocket != null) {
            webSocket.onopen = null;
            webSocket.onclose = null;
            webSocket.onmessage = null;
        }

        incomingMessage = [];
        outgoingMessage = [];

        //webSocket = new window.WebSocket(address, ["WSNetMQ"]);
        webSocket = new window.WebSocket(address, ["ZWS1.0"]);
        state = ConnectingState;

        webSocket.onopen = onopen;
        webSocket.onclose = onclose;
        webSocket.onmessage = onmessage;
    }

    function onopen (e) {
        console.log("WebSocket opened to " + address);
        reconnectTries = 0;

        state = ActiveState;

        if (that.activated != null) {
            that.activated(that);
        }
    };

    function onclose(e) {
        console.log("WebSocket closed",address, e.code,e.reason,e.wasClean);
        var stateBefore = state;
        state = ClosedState;

        if (stateBefore == ActiveState && that.deactivated != null) {
            that.deactivated(that);
        }
         window.setTimeout(open, 10000);
        // if (reconnectTries > 10) {
        //     window.setTimeout(open, 2000);
        // } else {
        //     open();
        // }
    };

    function onmessage(ev) {
        var more = ev.data[0];

        incomingMessage.push(ev.data.substr(1));

        // last message
        if (more == '0') {
            if (that.onMessage != null) {
                that.onMessage(that, incomingMessage);
            }

            incomingMessage = [];
        }
    };

    // activated event, when the socket is open
    this.activated = null;

    // deactivated event
    this.deactivated = null;

    this.onMessage = null;

    this.getIsActive = function() {
        return state == ActiveState;
    };

    this.write = function (frame, more) {
        if (more) {
            outgoingMessage.push("1" + frame);
        } else {
            outgoingMessage.push("0" + frame);

            for (var i = 0; i < outgoingMessage.length; i++) {
                webSocket.send(outgoingMessage[i]);
            }

            outgoingMessage = [];
        }
    };
}

// LoadBalancer

function LB() {
    var endpoints = [];
    var current = 0;
    var isActive = false;

    var inprogress = false;
    var dropping = false;

    this.writeActivated = null;

    var that = this;

    this.attach = function(endpoint) {
        endpoints.push(endpoint);

        if (!isActive) {
            isActive = true;

            if (that.writeActivated != null)
                that.writeActivated();
        }
    };

    this.terminated = function(endpoint) {
        var index = endpoints.indexOf(endpoint);

        if (current == index && inprogress) {
                dropping = true;
        }

        if (current == endpoints.length - 1) {
            current = 0;
        }

        endpoints.splice(index, 1);
    };

    this.send = function (message, more) {
        if (dropping) {
            inprogress = more;
            dropping = more;

            return true;
        }

        if (endpoints.length == 0) {
            isActive = false;
            return false;
        }

        endpoints[current].write(message, more);

        if (!more) {
            current = (current + 1) % endpoints.length;
        }

        return true;
    };

    this.getHasOut = function () {
        if (inprogress)
            return true;

        return endpoints.length > 0;
    };
}

// SocketBase Class

function SocketBase(xattachEndpoint, xendpointTerminated, xhasOut, xsend, xonMessage) {

    this.onMessage = null;
    this.sendReady = null;

    var endpoints = [];

    function onEndpointActivated(endpoint) {
        xattachEndpoint(endpoint);
    }

    function onEndpointDeactivated(endpoint) {
        xendpointTerminated(endpoint);
    }

    this.connect = function (address) {
        var endpoint = new Endpoint(address);
        endpoint.activated = onEndpointActivated;
        endpoint.deactivated = onEndpointDeactivated;
        endpoint.onMessage = xonMessage;
        endpoints.push(endpoint);
    };

    this.disconnect = function(address) {
        // TODO: implement disconnect
    };

    this.send = function (message, more) {
        more = typeof more !== 'undefined' ? more : false;

        return xsend(message, more);
    };

    this.getHasOut = function() {
        return xhasOut();
    };
}


function Dealer() {

    var lb = new LB();

    var that = new SocketBase(xattachEndpoint, xendpointTerminated, xhasOut, xsend, xonMessage);

    lb.writeActivated = function() {
        if (that.sendReady != null) {
            that.sendReady(that);
        }
    };

    function xattachEndpoint(endpoint) {
        lb.attach(endpoint);
    }

    function xendpointTerminated(endpoint) {
        lb.terminated(endpoint);
    }

    function xhasOut() {
        return lb.getHasOut();
    }

    function xsend(message, more) {
        return lb.send(message, more);
    }

    function xonMessage(endpoint, message) {
        if (that.onMessage != null) {
            that.onMessage(message);
        }
    }

    return that;
}

function Subscriber() {

    var that = new SocketBase(xattachEndpoint, xendpointTerminated, xhasOut, xsend, xonMessage);;

    var subscriptions = [];
    var endpoints = [];

    var isActive = false;

    that.subscribe = function (subscription) {
        // TODO: check if the subscription already exist
        subscriptions.push(subscription);

        for (var i = 0; i < endpoints.length; i++) {
            endpoints[i].write("1" + subscription);
        }
    }

    that.unsubscribe = function (subscription) {
        // TODO: check if the subscription even exist
        var index = subscriptions.indexOf(subscription);
        subscriptions.splice(index, 1);

        for (var i = 0; i < endpoints.length; i++) {
            endpoints[i].write("0" + subscription);
        }
    }

    function xattachEndpoint(endpoint) {
        endpoints.push(endpoint);

        for (var i = 0; i < subscriptions.length; i++) {
            endpoint.write("1" + subscriptions[i], false);
        }

        if (!isActive) {
            isActive = true;

            if (that.sendReady != null) {
                that.sendReady(that);
            }
        }
    }

    function xendpointTerminated(endpoint) {
        var index = endpoints.indexOf(endpoint);
        endpoints.splice(index, 1);
    }

    function xhasOut() {
        return false;
    }

    function xsend(message, more) {
        throw new "Send not supported on sub socket";
    }

    function xonMessage(endpoint, message) {
        if (that.onMessage != null) {
            that.onMessage(message);
        }
    }

    return that;
}