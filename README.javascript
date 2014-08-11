Experimental Javascript bindings, using ProtoBuf.js:
===================================================

see https://github.com/dcodeIO/ProtoBuf.js/wiki

install nodejs - see https://github.com/joyent/node/wiki/installing-node.js-via-package-manager

  sudo curl -sL https://deb.nodesource.com/setup | bash -
  sudo apt-get install nodejs

  npm install bytebuffer
  npm install ascli

  git clone https://github.com/dcodeIO/ProtoBuf.js.git

  add <dir>/ProtoBuf.js/bin to the PATH


then create the Javascript classes:

  make  PROTOBUFJS=1

browser code fragment:

<script src="./js/protobuf/Long.min.js"></script>
<script src="./js/protobuf/ByteBufferAB.min.js"></script>
<script src="./js/protobuf/ProtoBuf.min.js"></script>

<script src="./js/proto/types.js"></script>
<script src="./js/proto/message.js"></script>

        var msg = new pb.Container();
        msg.set_type(pb.ContainerType.MT_PING);
        msg.set_note(text.value);
	socket.send(msg.toArrayBuffer());
        console.log("hex: " + msg.toHex()+"\n");

