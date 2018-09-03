var machinetalkProtobuf = require('../index.js');

var encodedBuffer = new Buffer([0x08, 0xd2, 0x01]);

// Decode the message.
var decodedMessageContainer = machinetalkProtobuf.message.Container.decode(encodedBuffer);

// decodedMessageContainer.type === machinetalkProtobuf.message.ContainerType.MT_PING
console.log(decodedMessageContainer);
