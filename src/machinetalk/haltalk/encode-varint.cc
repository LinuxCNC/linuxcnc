#include <google/protobuf/io/coded_stream.h>
#include "../base/common.hh"

using namespace std;
using namespace google::protobuf::io;

inline const uint8* ReadVarint32FromArray(const uint8* buffer, uint32* value) {
  static const int kMaxVarintBytes = 10;
  static const int kMaxVarint32Bytes = 5;

  // Fast path:  We have enough bytes left in the buffer to guarantee that
  // this read won't cross the end, so we can skip the checks.
  const uint8* ptr = buffer;
  uint32 b;
  uint32 result;

  b = *(ptr++); result  = (b & 0x7F)      ; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |= (b & 0x7F) <<  7; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |= (b & 0x7F) << 14; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |= (b & 0x7F) << 21; if (!(b & 0x80)) goto done;
  b = *(ptr++); result |=  b         << 28; if (!(b & 0x80)) goto done;

  // If the input is larger than 32 bits, we still need to read it all
  // and discard the high-order bits.
  for (int i = 0; i < kMaxVarintBytes - kMaxVarint32Bytes; i++) {
    b = *(ptr++); if (!(b & 0x80)) goto done;
  }

  // We have overrun the maximum size of a varint (10 bytes).  Assume
  // the data is corrupt.
  return NULL;

 done:
  *value = result;
  return ptr;
}

int main() {
  static uint8 buffer[4];
  for (uint32 i = 0; i < 0xffffffff; ++i) {
    uint8* end = CodedOutputStream::WriteVarint32ToArray(i, buffer);
    uint32 value;
    ReadVarint32FromArray(buffer, &value);
    CHECK_EQ(i, value);
  }
  return 0;
}
