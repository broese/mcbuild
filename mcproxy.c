#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "lh_buffers.h"
#include "lh_files.h"
#include "lh_net.h"
#include "lh_event.h"

#define SERVER_IP 0x0a000001
//#define SERVER_IP 0xc7a866c2 // Beciliacraft
#define SERVER_PORT 25565

////////////////////////////////////////////////////////////////////////////////

int handle_server(int sfd, uint32_t ip, uint16_t port, int *cs, int *ms) {

    //// Handle Client-Side Connection

    // accept connection from the client side
    struct sockaddr_in cadr;
    int size = sizeof(cadr);
    *cs = accept(sfd, (struct sockaddr *)&cadr, &size);
    if (*cs < 0) LH_ERROR(0, "Failed to accept the client-side connection",strerror(errno));
    printf("Accepted from %s:%d\n",inet_ntoa(cadr.sin_addr),ntohs(cadr.sin_port));

    // set non-blocking mode on the client socket
    if (fcntl(*cs, F_SETFL, O_NONBLOCK) < 0) {
        close(*cs);
        LH_ERROR(0, "Failed to set non-blocking mode for the client",strerror(errno));
    }

    //// Handle Server-Side Connection

    // open connection to the remote server
    *ms = sock_client_ipv4_tcp(ip, port);
    if (*ms < 0) {
        close(*cs);
        LH_ERROR(0, "Failed to open the client-side connection",strerror(errno));
    }

    // set non-blocking mode
    if (fcntl(*ms, F_SETFL, O_NONBLOCK) < 0) {
        close(*cs);
        close(*ms);
        LH_ERROR(0, "Failed to set non-blocking mode for the server",strerror(errno));
    }

    return 1;
}

typedef struct {
    uint8_t * crbuf; ssize_t crlen;
    uint8_t * cwbuf; ssize_t cwlen;
    uint8_t * srbuf; ssize_t srlen;
    uint8_t * swbuf; ssize_t swlen;
} flbuffers;

#define WRITEBUF_GRAN 65536

ssize_t process_data(int is_client, uint8_t **sbuf, ssize_t *slen, uint8_t **dbuf, ssize_t *dlen) {
    ssize_t wpos = *dlen;
    hexdump(*sbuf, *slen);
    ARRAY_ADDG(*dbuf,*dlen,wpos+*slen,WRITEBUF_GRAN);
    memcpy(*dbuf+wpos,*sbuf,*slen);
    printf("copied %d bytes\n",*slen);
    return *slen;    
}

int pumpdata(int src, int dst, int is_client, flbuffers *fb) {
    printf("pumdata %d -> %d\n",src, dst);

    // choose correct buffers depending on traffic direction
    uint8_t **sbuf, **dbuf;
    ssize_t *slen, *dlen;

    if (is_client) {
        sbuf = &fb->crbuf;
        slen = &fb->crlen;
        dbuf = &fb->swbuf;
        dlen = &fb->swlen;
    }
    else {
        sbuf = &fb->srbuf;
        slen = &fb->srlen;
        dbuf = &fb->cwbuf;
        dlen = &fb->cwlen;
    }

    // read incoming data to the source buffer
    int res = evfile_read(src, sbuf, slen, 1048576);

    switch (res) {
        case EVFILE_OK:
        case EVFILE_WAIT: {
            // call the external method to process the input data
            // it will copy the data to dbuf as is, or modified, or it may
            // also insert other data
            ssize_t consumed = process_data(is_client, sbuf, slen, dbuf, dlen);
            if (consumed > 0) {
                if (*slen-consumed > 0)
                    memcpy(*sbuf, *sbuf+consumed, *slen-consumed);
                *slen -= consumed;
            }
            
            // send out the contents of the write buffer
            uint8_t *buf = *dbuf;
            ssize_t len = *dlen;
            while (len > 0) {
                //hexdump(buf,len);
                printf("sending %d bytes to %s\n",len,is_client?"server":"client");
                ssize_t sent = write(dst,buf,len);
                printf("sent %d bytes to %s\n",sent,is_client?"server":"client");
                buf += sent;
                len -= sent;
            }
            *dlen = 0;
            return 0;
        }         
        case EVFILE_ERROR:
        case EVFILE_EOF:
            return -1;
    }
}

int proxy_pump(uint32_t ip, uint16_t port) {
    printf("%s:%d\n",__func__,__LINE__);

    // setup server
    int ss = sock_server_ipv4_tcp_any(port);
    if (ss<0) return -1;

    pollarray pa; // common poll array for all sockets/files
    CLEAR(pa);

    pollgroup sg; // server listening socket
    CLEAR(sg);

    pollgroup cg; // client side socket
    CLEAR(cg);

    pollgroup mg; // server side socket
    CLEAR(mg);

    // add the server to listen for new client connections
    pollarray_add(&pa, &sg, ss, MODE_R, NULL);

    int cs=-1, ms=-1;

    flbuffers fb;
    CLEAR(fb);

    // main pump
    int stay = 1, i;
    while(stay) {
        // common poll for all registered sockets
        evfile_poll(&pa, 1000);

        printf("%s:%d sg:%d,%d,%d cg:%d,%d,%d mg:%d,%d,%d\n",
               __func__,__LINE__,
               sg.rn,sg.wn,sg.en,
               cg.rn,cg.wn,cg.en,
               mg.rn,mg.wn,mg.en);

        // handle connection requests from the client side
        if (sg.rn > 0 && cs == -1)
            if (handle_server(pa.p[sg.r[0]].fd, ip, port, &cs, &ms)) {
                printf("%s:%d - handle_server successful cs=%d ms=%d\n",
                       __func__,__LINE__, cs, ms);
                
                // handle_server was able to accept the client connection and
                // also open the server-side connection, we need to add these
                // new sockets to the groups cg and mg respectively
                pollarray_add(&pa, &cg, cs, MODE_R, NULL);
                pollarray_add(&pa, &mg, ms, MODE_R, NULL);
            }

        if (cg.rn > 0) {
            if (pumpdata(cs, ms, 1, &fb)) {
                pollarray_remove(&pa, cs);
                pollarray_remove(&pa, ms);
                close(cs); close(ms);
                cs = ms = -1;
                if (fb.crbuf) free(fb.crbuf);
                if (fb.cwbuf) free(fb.cwbuf);
                if (fb.srbuf) free(fb.srbuf);
                if (fb.swbuf) free(fb.swbuf);
                CLEAR(fb);
            }
        }
               
        if (mg.rn > 0) {
            if (pumpdata(ms, cs, 0, &fb)) {
                pollarray_remove(&pa, cs);
                pollarray_remove(&pa, ms);
                close(cs); close(ms);
                cs = ms = -1;
                if (fb.crbuf) free(fb.crbuf);
                if (fb.cwbuf) free(fb.cwbuf);
                if (fb.srbuf) free(fb.srbuf);
                if (fb.swbuf) free(fb.swbuf);
                CLEAR(fb);
            }
        }               
    }
}

int main(int ac, char **av) {
    proxy_pump(SERVER_IP, SERVER_PORT);

    return 0;
}
