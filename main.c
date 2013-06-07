#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <getopt.h>
#include <arpa/inet.h>
#include "event-struct.h"

struct state {
    int in_fd;
    int out_fd;
    int bytes;
    int verbose;
};

int add_header(struct state *st, int rows) {
    uint8_t magic[] = {0x54, 0x42, 0x45, 0x76};
    uint8_t magic2[] = {0x4d, 0x61, 0x44, 0x61};
    struct evt_file_header header;
    uint32_t offset;

    memset(&header, 0, sizeof(header));
    memcpy(header.magic1, magic, sizeof(magic));
    header.version = htonl(EVENT_VERSION);
    header.count   = htonl(rows);

    lseek(st->out_fd, 0, SEEK_SET);
    write(st->out_fd, &header, sizeof(header));

    // Write out the jump table.  For each row, allocate it a spot in the
    // jump table.
    for (offset=(sizeof(header)+(rows*sizeof(uint32_t)+sizeof(magic2)));
         rows;
         offset+=sizeof(struct evt_nand_read)) {
        uint32_t towrite = htonl(offset);
        write(st->out_fd, &towrite, sizeof(towrite));
        rows--;
    }

    write(st->out_fd, magic2, sizeof(magic2));
    return 0;
}


int do_convert(struct state *st) {
    int nand_pages;
    int current_page;
    int last_byte;
    int nsec;

    // Figure out how many pages there are.
    last_byte = lseek(st->in_fd, 0, SEEK_END);
    nand_pages = last_byte/st->bytes;
    lseek(st->in_fd, 0, SEEK_SET);
    
    add_header(st, nand_pages);

    // Now write the data out
    nsec = 0;
    current_page = 0;
    while (nand_pages) {
        union evt evt;
        memset(&evt, 0, sizeof(evt));
        evt.header.type = EVT_NAND_READ;
        evt.header.sec_start = htonl(0);
        evt.header.nsec_start = htonl(nsec);
        evt.header.sec_end = htonl(0);
        evt.header.nsec_end = htonl(nsec+1);
        evt.nand_read.addr[0] = htonl(0); // Col 0
        evt.nand_read.addr[1] = htonl(0); // Col 1
        evt.nand_read.addr[2] = (current_page)>>16;
        evt.nand_read.addr[3] = (current_page)>>8;
        evt.nand_read.addr[4] = (current_page)>>0;
        evt.nand_read.count = htonl(st->bytes);

        evt.header.size = 
            htonl(sizeof(struct evt_header)
                 + sizeof(evt.nand_read.addr)
                 + sizeof(evt.nand_read.count)
                 + sizeof(evt.nand_read.unknown)
                 + st->bytes);

        read(st->in_fd, evt.nand_read.data, st->bytes);
        write(st->out_fd, &evt, ntohl(evt.header.size));
        current_page++;
        nand_pages--;
        nsec += 5;
    }

    close(st->in_fd);
    close(st->out_fd);
    return 0;
}


int print_usage(char *name) {
    printf("Usage:\n"
           "\t%s -i [ramfile] -o [tbevent] -v\n"
           " -i [ramfile]  Source ram stream filename\n"
           " -o [tbraw]    Target tbraw filenmaje\n"
           " -b [size]     Number of bytes in a page (including address bytes)\n"
           , name);
    return 0;
}

int main(int argc, char **argv) {
    int opt;
    struct state st;

    memset(&st, 0, sizeof(st));
    st.bytes = 0x23d0;

    while ((opt = getopt(argc, argv, "i:o:s:v")) != -1) {
        switch(opt) {
            case 'i':
                if (st.in_fd)
                    close(st.in_fd);
                st.in_fd = open(optarg, O_RDONLY);
                if (-1 == st.in_fd) {
                    perror("Unable to open ram stream");
                    return 1;
                }
                break;

            case 'o':
                if (st.out_fd)
                    close(st.out_fd);
                st.out_fd = open(optarg, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (-1 == st.out_fd) {
                    perror("Unable to open output stream");
                    return 1;
                }
                break;

            case 'v':
                st.verbose++;
                break;

            case 'b':
                st.bytes = strtoul(optarg, NULL, 0);
                break;

            case 'h':
            default:
                return print_usage(argv[0]);
        }
    }

    if (!st.out_fd || !st.in_fd)
        return print_usage(argv[0]);

    argv += optind;
    argc -= optind;

    return do_convert(&st);
}
