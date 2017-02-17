var machinetalkProtobuf = require('../index.js');

// Define the message we want to encode.
var messageContainer = {
  type: machinetalkProtobuf.message.ContainerType.MT_PING
};

// Encode the message.
var encodedMessageContainer = machinetalkProtobuf.message.Container.encode(messageContainer);

// Strip off excess bytes from the resulting buffer.
var encodedBuffer = encodedMessageContainer.buffer.slice(encodedMessageContainer.offset, encodedMessageContainer.limit);

// Print the buffer.
console.log(encodedBuffer);