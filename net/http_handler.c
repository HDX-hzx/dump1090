#include "dump1090.h"

/* Return a description of planes in json. */
char *aircraftsToJson(int *len) {
    struct aircraft *a = Modes.aircrafts;
    int buflen = 1024; /* The initial buffer is incremented as needed. */
    char *buf = malloc(buflen), *p = buf;
    int l;

    if (buf == NULL) {
        *len = 0;
        return NULL;
    }

    l = snprintf(p,buflen,"[\n");
    p += l; buflen -= l;
    while(a) {
        int altitude = a->altitude, speed = a->speed;

        /* Convert units to metric if --metric was specified. */
        if (Modes.metric) {
            altitude /= 3.2828;
            speed *= 1.852;
        }

        if (a->lat != 0 && a->lon != 0) {
            l = snprintf(p,buflen,
                "{\"hex\":\"%s\", \"flight\":\"%s\", \"lat\":%f, "
                "\"lon\":%f, \"altitude\":%d, \"track\":%d, "
                "\"speed\":%d},\n",
                a->hexaddr, a->flight, a->lat, a->lon, altitude, a->track,
                speed);
            p += l; buflen -= l;
            /* Resize if needed. */
            if (buflen < 256) {
                int used = p-buf;
                char *newbuf;
                buflen += 1024; /* Our increment. */
                newbuf = realloc(buf,used+buflen);
                if (newbuf == NULL) {
                    /* Keep what we have so far rather than losing the
                     * whole response and leaking the original buffer. */
                    break;
                }
                buf = newbuf;
                p = buf+used;
            }
        }
        a = a->next;
    }
    /* Remove the final comma if any, and closes the json array. */
    if (*(p-2) == ',') {
        *(p-2) = '\n';
        p--;
        buflen++;
    }
    l = snprintf(p,buflen,"]\n");
    p += l; buflen -= l;

    *len = p-buf;
    return buf;
}

#define MODES_CONTENT_TYPE_HTML "text/html;charset=utf-8"
#define MODES_CONTENT_TYPE_JSON "application/json;charset=utf-8"

/* Get an HTTP request header and write the response to the client.
 * Again here we assume that the socket buffer is enough without doing
 * any kind of userspace buffering.
 *
 * Returns 1 on error to signal the caller the client connection should
 * be closed. */
int handleHTTPRequest(struct client *c) {
    char hdr[512];
    int clen, hdrlen;
    int httpver, keepalive;
    char *p, *url, *content;
    char *ctype;

    if (Modes.debug & MODES_DEBUG_NET)
        printf("\nHTTP request: %s\n", c->buf);

    /* Minimally parse the request. */
    httpver = (strstr(c->buf, "HTTP/1.1") != NULL) ? 11 : 10;
    if (httpver == 10) {
        /* HTTP 1.0 defaults to close, unless otherwise specified. */
        keepalive = strstr(c->buf, "Connection: keep-alive") != NULL;
    } else if (httpver == 11) {
        /* HTTP 1.1 defaults to keep-alive, unless close is specified. */
        keepalive = strstr(c->buf, "Connection: close") == NULL;
    }

    /* Identify he URL. */
    p = strchr(c->buf,' ');
    if (!p) return 1; /* There should be the method and a space... */
    url = ++p; /* Now this should point to the requested URL. */
    p = strchr(p, ' ');
    if (!p) return 1; /* There should be a space before HTTP/... */
    *p = '\0';

    if (Modes.debug & MODES_DEBUG_NET) {
        printf("\nHTTP keep alive: %d\n", keepalive);
        printf("HTTP requested URL: %s\n\n", url);
    }

    /* Select the content to send, we have just two so far:
     * "/" -> Our google map application.
     * "/data.json" -> Our ajax request to update planes. */
    if (strstr(url, "/data.json")) {
        content = aircraftsToJson(&clen);
        if (content == NULL) {
            content = strdup("[]\n");
            clen = strlen(content);
        }
        ctype = MODES_CONTENT_TYPE_JSON;
    } else {
        struct stat sbuf;
        int fd = -1;

        if (stat("gmap.html",&sbuf) != -1 &&
            (fd = open("gmap.html",O_RDONLY)) != -1)
        {
            /* Guard against a zero-byte or otherwise unusable file size:
             * malloc(0) is implementation-defined and, combined with the
             * unconditional read()/snprintf() below, could end up writing
             * through a NULL or zero-length pointer. */
            if (sbuf.st_size <= 0) {
                close(fd);
                fd = -1;
                content = strdup("Error: gmap.html is empty or unreadable");
                clen = strlen(content);
            } else {
                content = malloc(sbuf.st_size);
                if (content == NULL) {
                    close(fd);
                    fd = -1;
                    content = strdup("Error: out of memory serving gmap.html");
                    clen = strlen(content);
                } else if (read(fd,content,sbuf.st_size) == -1) {
                    snprintf(content,sbuf.st_size,"Error reading from file: %s",
                        strerror(errno));
                    clen = strlen(content);
                } else {
                    clen = sbuf.st_size;
                }
            }
        } else {
            char buf[128];

            clen = snprintf(buf,sizeof(buf),"Error opening HTML file: %s",
                strerror(errno));
            content = strdup(buf);
        }
        if (fd != -1) close(fd);
        ctype = MODES_CONTENT_TYPE_HTML;
    }

    /* Create the header and send the reply. */
    hdrlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Server: Dump1090\r\n"
        "Content-Type: %s\r\n"
        "Connection: %s\r\n"
        "Content-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        ctype,
        keepalive ? "keep-alive" : "close",
        clen);

    if (Modes.debug & MODES_DEBUG_NET)
        printf("HTTP Reply header:\n%s", hdr);

    /* Send header and content. */
    if (write(c->fd, hdr, hdrlen) != hdrlen ||
        write(c->fd, content, clen) != clen)
    {
        free(content);
        return 1;
    }
    free(content);
    Modes.stat_http_requests++;
    return !keepalive;
}

