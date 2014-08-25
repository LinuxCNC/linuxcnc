/* pb.c -- decode a protobuf into its field numbers and wire values.
 *
 * This decoder does not require a .proto file.  It is equivalent to a
 * generic XML parser, in that it can parse the file, but doesn't know
 * semantically what any of it means.  Also like XML, it doesn't know whether
 * a 32-bit value is an integer or a float, signed or unsigned.  Again, this
 * is much like XML, where you are similarly in the dark if you see a fragment
 * like <foo>332</foo>.
 *
 * Joshua Haberman <joshua@reverberate.org>
 */

#include <stdint.h>
#include <stdio.h>

enum wire_type {
    WT_VARINT = 0,
    WT_64BIT  = 1,
    WT_STRING = 2,
    WT_32BIT  = 5
};

/* Structure representing a tag number and its corresponding wire value.
 * With a .proto file we can refine this data by:
 * - translating field_number -> field_name
 * - translating the wire value into the specific value (ex. 32-bit -> float)
 */
struct key_value_pair
{
    int field_number;
    enum wire_type wire_type;
    union {
      uint64_t varint;
      uint64_t _64_bit;
      struct {
          char *start;
          int len;
      } string;
      uint32_t _32_bit;
    } value;
};

/* A callback that is called every time we parse a key/value pair */
typedef void (*yield_t)(struct key_value_pair*);

uint64_t decode_varint(char *buf, char **end)
{
    uint64_t ret = 0;
    int bitpos = 0;
    for(int bitpos = 0; *buf & 0x80 && bitpos < 64; bitpos += 7, buf++)
        ret |= (*buf & 0x7F) << bitpos;
    ret |= (*buf & 0x7F) << bitpos;
    *end = buf+1;
    return ret;
}

uint32_t get_32_le(char *buf, char **end)
{
    *end = buf+4;
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

void decode_protobuf(char *buf, char *buf_end, yield_t yield)
{
    while(buf < buf_end)
    {
        uint64_t key = decode_varint(buf, &buf);
        struct key_value_pair pair = {
            .field_number = key >> 3,
            .wire_type = key & 0x07
        };
        switch(pair.wire_type)
        {
            case WT_VARINT:  /* varint */
                pair.value.varint = decode_varint(buf, &buf);
                break;

            case WT_64BIT:  /* 64 bit */
                pair.value._64_bit = get_32_le(buf, &buf);
                buf += 4;
                pair.value._64_bit |= (uint64_t)get_32_le(buf, &buf) << 32;
                buf += 4;
                break;

            case WT_STRING:  /* string */
                pair.value.string.len = decode_varint(buf, &buf);
                pair.value.string.start = buf;
                buf += pair.value.string.len;
                break;

            case WT_32BIT:  /* 32 bit */
                pair.value._32_bit = get_32_le(buf, &buf);
                buf += 4;
                break;
        }

        yield(&pair);
    }
}

void yield_cb(struct key_value_pair *pair)
{
    printf("Field number: %d, Wire type: %d\n", pair->field_number,
                                                pair->wire_type);
}

int main()
{
    char pb_1[] = {0x12, 0x07, 0x74, 0x65, 0x73, 0x74, 0x69, 0x6e, 0x67};
    char pb_2[] = {0x08, 0x96, 0x01};
    decode_protobuf(pb_1, pb_1+sizeof(pb_1), yield_cb);
    decode_protobuf(pb_2, pb_2+sizeof(pb_2), yield_cb);
}
