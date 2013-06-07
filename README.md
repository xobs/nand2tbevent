nand2tbevent
============

Convert raw NAND dumps into tbevent streams.

tbevent format
--------------

The tbevent stream contains three parts: A header, a jump table, and the
event streams.  All data in the file is in network byte-order.

The header contains a packed struct, that looks like this:

    struct evt_file_header {
        uint8_t magic1[4];  // {0x54, 0x42, 0x45, 0x76};
        uint32_t version;   // Version 1
        uint32_t count;     // Number of events in the stream
        uint32_t reserved1; // Set to 0
        uint32_t reserved2; // Set to 0
        uint32_t reserved3; // Set to 0
        uint32_t reserved4; // Set to 0
    };

Following the file header is an array of uint32_t pointers.  Each pointer
points to the start of an event, and contains an offset relative to the
start of the file.

Following the jump table is the magic sequence {0x4d, 0x61, 0x44, 0x61}.

After this second magic sequence, you can find a series of event structures.
Every event struct has the same header:

    struct evt_header {
        uint8_t type;           // See event-struct.h
        uint32_t sec_start;     // When this event started
        uint32_t nsec_start;    // Nanoseconds when it started
        uint32_t sec_end;       // When it ended
        uint32_t nsec_end;      // Nanoseconds when it ended
        uint32_t size;          // Total size of packet (including header)
    };

