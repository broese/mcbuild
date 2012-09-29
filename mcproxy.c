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

/*
typedef struct {
    uint8_t * data;
    ssize_t len;
} buffer_t;
*/

int handle_server(int sfd, uint32_t ip, uint16_t port, FILE **csock, FILE **msock) {

    //// Handle Client-Side Connection

    // accept connection from the client side
    struct sockaddr_in cadr;
    int size = sizeof(cadr);
    int cl = accept(sfd, (struct sockaddr *)&cadr, &size);
    if (cl < 0) LH_ERROR(0, "Failed to accept the client-side connection",strerror(errno));
    printf("Accepted from %s:%d\n",inet_ntoa(cadr.sin_addr),ntohs(cadr.sin_port));

    // set non-blocking mode on the client socket
    if (fcntl(cl, F_SETFL, O_NONBLOCK) < 0) {
        close(cl);
        LH_ERROR(0, "Failed to set non-blocking mode for the client",strerror(errno));
    }

    // make it to a FILE *
    *csock = fdopen(cl, "r+");
    if (! *csock) {
        close(cl);
        LH_ERROR(0, "Failed to fdopen the client socket",strerror(errno));
    }



    //// Handle Server-Side Connection

    // open connection to the remote server
    int ms = sock_client_ipv4_tcp(ip, port);
    if (ms < 0) {
        fclose(*csock);
        *csock = NULL;
        LH_ERROR(0, "Failed to open the client-side connection",strerror(errno));
    }

    // set non-blocking mode
    if (fcntl(ms, F_SETFL, O_NONBLOCK) < 0) {
        fclose(*csock);
        *csock = NULL;
        close(ms);
        LH_ERROR(0, "Failed to set non-blocking mode for the server",strerror(errno));
    }

    // make it to a FILE *
    *msock = fdopen(ms, "r+");
    if (! *msock) {
        fclose(*csock);
        *csock = NULL;
        close(ms);
        LH_ERROR(0, "Failed to fdopen the server-side socket",strerror(errno));
    }

    return 1;

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

    FILE *csock = NULL, *msock = NULL;

    // growable buffers
    ARRAY(uint8_t,crbuf,crlen);
    //ARRAY(uint8_t,cwbuf,cwlen);
    ARRAY(uint8_t,srbuf,srlen);
    //ARRAY(uint8_t,swbuf,swlen);

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
        if (sg.rn > 0 && csock == NULL)
            if (handle_server(pa.p[sg.r[0]].fd, ip, port, &csock, &msock)) {
                printf("%s:%d - handle_server successful cs=%d ms=%d\n",
                       __func__,__LINE__, fileno(csock), fileno(msock));
                
                // handle_server was able to accept the client connection and
                // also open the server-side connection, we need to add these
                // new sockets to the groups cg and mg respectively
                pollarray_add_file(&pa, &cg, csock, MODE_R, NULL);
                pollarray_add_file(&pa, &mg, msock, MODE_R, NULL);
            }
        
        if (cg.rn > 0) {
            printf("%s:%d\n",__func__,__LINE__);
            // incoming data from the client side
            FILE * fd = pa.files[cg.r[0]];
            printf("%s:%d fd=%d fd=%08x\n",__func__,__LINE__,fd);
            printf("%s:%d fd=%d\n",__func__,__LINE__,fileno(fd));
            int res = evfile_read(fileno(fd), &crbuf, &crlen, 1048576);

            switch (res) {
                case EVFILE_OK:
                case EVFILE_WAIT: {
                    uint8_t *buf = crbuf;
                    ssize_t len = crlen;
                    while (len > 0) {
                        hexdump(buf,len);
                        printf("%s:%d len=%d\n",__func__,__LINE__,len);
                        printf("sending %d bytes to server\n",len);
                        ssize_t sent = write(fileno(msock),buf,len);
                        printf("sent %d bytes to server\n",sent);
                        buf += sent;
                        len -= sent;
                    }
                    crlen = 0;
                    break;
                }         
                case EVFILE_ERROR:
                    printf("Error occurred on fd=%d\n",fileno(fd));
                case EVFILE_EOF:
                    printf("Closing client connection for fd=%d\n",fileno(fd));
                    crlen = 0;
                    pollarray_remove_file(&pa, fd);
                    break;
            }
        }
            
        if (mg.rn > 0) {
            printf("%s:%d\n",__func__,__LINE__);
            // incoming data from the client side
            FILE * fd = pa.files[mg.r[0]];
            printf("%s:%d fd=%d fd=%08x\n",__func__,__LINE__,fd);
            printf("%s:%d fd=%d\n",__func__,__LINE__,fileno(fd));
            int res = evfile_read(fileno(fd), &srbuf, &srlen, 1048576);

            switch (res) {
                case EVFILE_OK:
                case EVFILE_WAIT: {
                    uint8_t *buf = srbuf;
                    ssize_t len = srlen;
                    while (len > 0) {
                        hexdump(buf,len);
                        printf("%s:%d len=%d\n",__func__,__LINE__,len);
                        printf("sending %d bytes to client\n",len);
                        ssize_t sent = write(fileno(csock),buf,len);
                        printf("sent %d bytes to client\n",sent);
                        buf += sent;
                        len -= sent;
                    }
                    srlen = 0;
                    break;
                }         
                case EVFILE_ERROR:
                    printf("Error occurred on fd=%d\n",fileno(fd));
                case EVFILE_EOF:
                    printf("Closing server connection for fd=%d\n",fileno(fd));
                    srlen = 0;
                    pollarray_remove_file(&pa, fd);
                    break;
            }
        }
    }
}

int main(int ac, char **av) {
    proxy_pump(SERVER_IP, SERVER_PORT);

    return 0;
}
