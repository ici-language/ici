#define ICI_CORE
/*
 * skt.c - ICI sockets interface
 *
 * This is a set of extra intrinsic functions to provide ICI programs
 * with access to the BSD sockets interface to the IP protocol (there is
 * currently no support for other protocol families but it is easily
 * provided.)
 *
 * The ICI interface does not follow the BSD C interface but adopts
 * practices suitable for the ICI environment.
 */

#include <fwd.h>
#ifndef NOSKT
#include "null.h"
#include "skt.h"
#include "primes.h"

#include <string.h>
#include <errno.h>

#ifdef _WIN32

/*
 * To use sockets under WIN32 requires explicit initialisation.
 * Here's the code and data to do that and a macro to call the
 * initialisation routine if it hasn't already been called. For
 * the BSD/Unix implementation the macro is empty.
 *
 * N.B We can refer to WINSOCK defined things here as skt.h includes
 * winsock.h so it can get the definition of SOCKET.
 */

static WSADATA          wsadata;
static int              need_init_winsock = 1;

static int
init_winsock(void)
{
    if (WSAStartup(MAKEWORD(1,1), &wsadata))
    {
        ici_error = "failed to initialise WINSOCK";
        return 1;
    }
    need_init_winsock = 0;
    return 0;
}

#define INITWINSOCK()   if (need_init_winsock && init_winsock()) return 1

/*
 * The f_hostname() function needs to know how long a host name may
 * be. WINSOCK doesn't seem to want to tell us.
 */
#define MAXHOSTNAMELEN (64)

#else   /* #ifdef _WIN32 */

#include <sys/param.h>

/*
 * For ease of compatibility with WINSOCK we use its definitions and
 * emulate them on Unix. Luckily such emulation is rather trivial.
 */
#define SOCKET          int
#define closesocket     close
#define INITWINSOCK()
#define SOCKET_ERROR    (-1)

#undef isset /* sys/param.h defines set macros that conflict with ICI's */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h> /* for TCP_NODELAY (on most systems) */
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pwd.h>
#if __hpux || BSD4_4 || linux
#include <unistd.h>
#endif
#if NeXT
#include <libc.h>
#endif
#if sun
#define bzero(p,n)      (memset((p), 0, (n)))
#endif
#endif  /* #ifdef _WIN32 */

#include "buf.h"
#include "exec.h"
#include "op.h"
#include "func.h"
#include "int.h"
#include "set.h"
#include "struct.h"
#include "str.h"
#include "primes.h"
#include "file.h"

static string_t *string_n;
static string_t *string_read;
static string_t *string_write;
static string_t *string_except;
static string_t *string_msg;
static string_t *string_addr;

#if defined(ULTRIX) || (defined(BSD4_4) && !defined(INADDR_LOOPBACK))
/*
 * Local loop back IP address (127.0.0.1) in host byte order.
 */
#define INADDR_LOOPBACK         (u_long)0x7F000001
#endif

#if defined(sun) && __STDC__
/*
 * SunOS 4.x doesn't prototypes many things.
 */
int close(int);
int socket(int, int, int);
int socketpair(int, int, int, int *);
int listen(int, int);
int accept(int, struct sockaddr *, int *);
int connect(int, struct sockaddr *, int);
#ifndef SUNOS5
int bind(int, struct sockaddr *, int);
int sendto(int, char *, int, int, struct sockaddr *, int);
#endif
#ifndef SUNOS5
int send(int, char *, int, int);
#endif
int recvfrom(int, char *, int, int, struct sockaddr *, int *);
int recv(int, char *, int, int);
int getsockopt(int, int, int, char *, int *);
#ifndef SUNOS5
int setsockopt(int, int, int, char *, int);
#endif
int gethostname(char *, int);
int getuid(void);
int getpeername(int, struct sockaddr *, int *);
int getsockname(int, struct sockaddr *, int *);
int shutdown(int, int);
#endif

/*
 * Definitions to make a new type, socket, for representing network connections
 */

/*
 * Mark this and referenced unmarked objects, return memory costs.
 * See comments on t_mark() in object.h.
 */
static unsigned long
mark_socket(object_t *o)
{
    o->o_flags |= O_MARK;
    return sizeof (skt_t);
}

/*
 * Free this object and associated memory (but not other objects).
 * See the comments on t_free() in object.h.
 */
static void
free_socket(object_t *o)
{
    if (!sktof(o)->s_closed)
        closesocket(sktof(o)->s_skt);
    ici_tfree(o, skt_t);
}

/*
 * Return a hash sensitive to the value of the object.
 * See the comment on t_hash() in object.h
 */
static unsigned long
hash_socket(object_t *o)
{
    return sktof(o)->s_skt * SKT_PRIME;
}

/*
 * Returns 0 if these objects are equal, else non-zero.
 * See the comments on t_cmp() in object.h.
 */
static int
cmp_socket(object_t *o1, object_t *o2)
{
    return sktof(o1)->s_skt != sktof(o2)->s_skt
        || sktof(o1)->s_closed != sktof(o2)->s_closed;
}

type_t  socket_type =
{
    mark_socket,
    free_socket,
    hash_socket,
    cmp_socket,
    copy_simple,
    assign_simple,
    fetch_simple,
    "socket"
};

/*
 * Lookup a socket with a given descriptor in the atom table. If it exists
 * we use it as sockets are defined to be atomic.
 */
static skt_t *
atom_skt(SOCKET fd)
{
    register object_t   *o;
    register object_t   **po;

    for
    (
        po = &atoms[ici_atom_hash_index((unsigned long)fd * SKT_PRIME)];
        (o = *po) != NULL;
        --po < atoms ? po = atoms + atomsz - 1 : NULL
    )
    {
        if (isskt(o) && sktof(o)->s_skt == fd && !sktof(o)->s_closed)
            return sktof(o);
    }
    return NULL;
}

/*
 * Create a new socket object with the given descriptor.
 */
skt_t *
new_socket(SOCKET fd)
{
    skt_t       *s;

    if ((s = atom_skt(fd)) != NULL)
    {
        incref(s);
        return s;
    }
    if ((s = ici_talloc(skt_t)) == NULL)
        return NULL;
    objof(s)->o_tcode = TC_SOCKET;
    assert(ici_typeof(s) == &socket_type);
    objof(s)->o_flags = 0;
    objof(s)->o_nrefs = 1;
    s->s_skt = fd;
    s->s_closed = 0;
    rego(s);
    return sktof(atom(objof(s), 1));
}

static int
isclosed(skt_t *s)
{
    if (s->s_closed)
    {
        ici_error = "attempt to use closed socket";
        return 1;
    }
    return 0;
}

int
skt_close(skt_t *s)
{
    if (isclosed(s))
        return 1;
    closesocket(s->s_skt);
    s->s_closed = 1;
    return 0;
}

static int need_strings = 1;
static int
define_strings(void)
{
    if ((string_n = new_cname("n")) == NULL)
        return 1;
    if ((string_read = new_cname("read")) == NULL)
        return 1;
    if ((string_write = new_cname("write")) == NULL)
        return 1;
    if ((string_except = new_cname("except")) == NULL)
        return 1;
    if ((string_msg = new_cname("msg")) == NULL)
        return 1;
    if ((string_addr = new_cname("addr")) == NULL)
        return 1;
    return need_strings = 0;
}

/*
 * Parse an IP address in the format "service[@host]" where service is a
 * port number of service name (in the form "name[/proto]" where name is
 * the name of the service and, the optional, proto is either "tcp" or
 * "udp"). The host part is optional and if not specified defaults to
 * the defhost parameter. The host may be specified as an IP address
 * in dotted decimal notation or as a hostname. Three special values
 * are recognsied, "." stands for the local host, "?" stands for any
 * host and "*" means broadcast. The address may be NULL to just
 * initialise the socket address to defhost port 0.
 *
 * The sockaddr structure is filled in and 0 returned if all is okay.
 * When a error occurs the error string is set and 1 is returned.
 */
static struct sockaddr_in *
parseaddr(raddr, defhost, saddr)
char *raddr;
long defhost;
struct sockaddr_in *saddr;
{
    char addr[1024];
    char *host;
    short port;

    saddr->sin_family = PF_INET;
    saddr->sin_addr.s_addr = htonl(defhost);
    saddr->sin_port = 0;
    if (raddr == NULL)
        return saddr;
    strcpy(addr, raddr);
    if ((host = strchr(addr, '@')) != NULL)
    {
        struct hostent *hostent;
        long hostaddr;

        *host++ = 0;
        if (!strcmp(host, "."))
            hostaddr = htonl(INADDR_LOOPBACK);
        else if (!strcmp(host, "?"))
            hostaddr = htonl(INADDR_ANY);
        else if (!strcmp(host, "*"))
            hostaddr = htonl(INADDR_BROADCAST);
        else if ((hostaddr = inet_addr(host)) != (unsigned long)-1)
            /* NOTHING */ ;
        else if ((hostent = gethostbyname(host)) != NULL)
            memcpy(&hostaddr, hostent->h_addr, sizeof hostaddr);
        else
        {
            ici_error = "unknown host";
            return NULL;
        }
        saddr->sin_addr.s_addr = hostaddr;
    }
    if (sscanf(addr, "%hu", &port) != 1)
    {
        char *proto;
        struct servent *servent;
        if ((proto = strchr(addr, '/')) != NULL)
            *proto++ = 0;
        if ((servent = getservbyname(addr, proto)) == NULL)
        {
            ici_error = "unknown service";
            return NULL;
        }
        port = ntohs(servent->s_port);
    }
    saddr->sin_port = htons(port);
    return saddr;
}

/*
 * Turn a port number and IP address into a nice looking string ;-)
 */
static char *
unparse_addr(addr)
struct sockaddr_in *addr;
{
    static char addr_buf[256];
    struct servent *serv;
    struct hostent *host;

    if ((serv = getservbyport(addr->sin_port, NULL)) != NULL)
        strcpy(addr_buf, serv->s_name);
    else
        sprintf(addr_buf, "%u", ntohs(addr->sin_port));
    strcat(addr_buf, "@");
    if (addr->sin_addr.s_addr == INADDR_ANY)
        strcat(addr_buf, "?");
    else if (addr->sin_addr.s_addr == INADDR_LOOPBACK)
        strcat(addr_buf, ".");
    else if (addr->sin_addr.s_addr == INADDR_BROADCAST)
        strcat(addr_buf, "*");
    else if
    (
        (
            host
            =
            gethostbyaddr
            (
                (char *)&addr->sin_addr.s_addr,
                sizeof addr->sin_addr,
                AF_INET
            )
        )
        ==
        NULL
    )
        strcat(addr_buf, inet_ntoa(addr->sin_addr));
    else
        strcat(addr_buf, host->h_name);
    return addr_buf;
}

/*
 * Error return utility. Formats the exception string with the
 * the message followed by the system error message and returns
 * 1 for use in error returns.
 */
static int
serr(msg)
char *msg;
{
    sprintf(buf, "%s: %s", msg, syserr());
    ici_error = buf;
    errno = 0;
    return 1;
}

/*
 * Create a socket with a certain protocol (currently TCP or UDP)
 * and return its descriptor. Raises exception if the protocol
 * is unknown or the socket cannot be created.
 *
 * ICI usage,
 *
 *      skt = socket(proto);
 *
 * Where proto one of the strings "tcp", "tcp/ip", "udp" or "udp/ip".
 * The "/ip" is the start of handling different protocol families (as
 * implemented in BSD and WINSOCK 2). For compatibiliy with exisitng
 * ICI sockets code the default protocol family is defined to be "ip".
 *
 * Returns a socket object representing a communications end-point.
 */
static int
f_socket(void)
{
    skt_t       *skt;
    char        *proto;
    int         type;
    SOCKET      fd;

    INITWINSOCK();
    if (ici_typecheck("s", &proto))
        return 1;
    if (!strcmp(proto, "tcp") || !strcmp(proto, "tcp/ip"))
        type = SOCK_STREAM;
    else if (!strcmp(proto, "udp") || !strcmp(proto, "udp/ip"))
        type = SOCK_DGRAM;
    else
    {
        ici_error = "unsupported protocol or family";
        return 1;
    }
    if ((fd = socket(PF_INET, type, 0)) == -1)
        return serr("socket");
    if ((skt = new_socket(fd)) == NULL)
    {
        closesocket(fd);
        return 1;
    }
    return ici_ret_with_decref(objof(skt));
}

/*
 * Tell the system to listen for connections on a socket. Raises
 * the appropriate system error if it fails.
 *
 * ICI usage,
 *
 *      skt = listen(skt [, backlog]);
 *
 * Where skt is the socket descriptor as returned by socket(). The
 * result of listen is its parameter (to allow "functional" programming.)
 *
 */
static int
f_listen(void)
{
    skt_t       *skt;
    long        backlog = 5;

    switch (NARGS())
    {
    case 1:
        if (ici_typecheck("o", &skt))
            return 1;
        break;

    case 2:
        if (ici_typecheck("oi", &skt, &backlog))
            return 1;
        break;

    default:
        return ici_argcount(2);
    }
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (isclosed(skt))
        return 1;
    if (backlog < 1)
    {
        ici_error = "invalid backlog value in listen()";
        return 1;
    }
    if (listen(skt->s_skt, (int)backlog) == -1)
        return serr("listen");
    return ici_ret_no_decref(objof(skt));
}

/*
 * Accept a connection on a socket. Returns the descriptor for the
 * new socket connection or raises an exception.
 *
 * ICI usage,
 *
 *      new_skt = accept(skt);
 *
 * Where skt is a socket descriptor of a TCP socket that has been
 * marked to accept connections (i.e., been passed to listen()).
 * The result is the socket descriptor of the new connection.
 */
static int
f_accept(void)
{
    skt_t       *skt;
    SOCKET      fd;

    if (ici_typecheck("o", &skt))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (isclosed(skt))
        return 1;
    if ((fd = accept(skt->s_skt, NULL, NULL)) == -1)
        return serr("accept");
    return ici_ret_with_decref(objof(new_socket(fd)));
}

/*
 * Connect a socket to an address. Raises an exception if it fails
 * for some reason or returns the socket passed as the first
 * parameter which is now connected to the address specified as
 * the second parameter.
 *
 * ICI usage,
 *
 *      skt = connect(skt, address);
 *
 * Skt is a socket desciptor and address is a network address as
 * accepted by parseaddr().
 *
 */
static int
f_connect(void)
{
    skt_t               *skt;
    char                *addr;
    object_t            *arg;
    struct sockaddr_in  sockaddr;

    if (ici_typecheck("oo", &skt, &arg))
        return 1;
    if (isstring(arg))
        addr = stringof(arg)->s_chars;
    else if (isint(arg))
    {
        sprintf(buf, "%ld", intof(arg)->i_value);
        addr = buf;
    }
    else
        return ici_argerror(1);
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (parseaddr(addr, INADDR_LOOPBACK, &sockaddr) == NULL)
        return 1;
    if (isclosed(skt))
        return 1;
    if (connect(skt->s_skt, (struct sockaddr *)&sockaddr, sizeof sockaddr) == -1)
        return serr("connect");
    return ici_ret_no_decref(objof(skt));
}

/*
 * Bind the local address of a socket and returns socket descriptor.
 *
 * ICI usage,
 *
 *      skt = bind(skt [, address]);
 *
 * If no address is passed the system allocates a local address (i.e., for
 * a TCP socket we pass zero as the port number to bind() and have it select
 * a local port number). If address is passed but has no host portion then
 * the default host address is INADDR_ANY which is usually what you want to
 * do for servers (i.e. a server can use the code "bind(socket("tcp"), port)"
 * to create their sockets which can accept connections originating from any
 * network interface).
 */
static int
f_bind(void)
{
    skt_t               *skt;
    char                *addr;
    struct sockaddr_in  sockaddr;

    if (NARGS() == 2)
    {
        skt = sktof(ARG(0));
        if (isstring(ARG(1)))
        {
            addr = stringof(ARG(1))->s_chars;
        }
        else if (isint(ARG(1)))
        {
            sprintf(buf, "%ld", intof(ARG(1))->i_value);
            addr = buf;
        }
        else
        {
            return ici_argerror(1);
        }
    }
    else
    {
        if (ici_typecheck("o", &skt))
            return 1;
        addr = "0";
    }
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (parseaddr(addr, INADDR_ANY, &sockaddr) == NULL)
        return 1;
    if (isclosed(skt))
        return 1;
    if (bind(skt->s_skt, (struct sockaddr *)&sockaddr, sizeof sockaddr) == -1)
        return serr("bind");
    return ici_ret_no_decref(objof(skt));
}

/*
 * Helper function for f_select(). Adds a set of ready socket descriptors
 * to the struct object returned by f_select() by scanning the descriptors
 * actually selected and seeing if they were returned as being ready. If
 * so they are added to a set. Finally the set is added to the struct
 * object. Uses, and updates, the count of ready descriptors that is
 * returned by select(2) so we can avoid unneccesary work.
 */
static int
select_add_result
(
    struct_t            *result,
    string_t            *key,
    set_t               *set,
    fd_set              *fds,
    int                 *n
)
{
    set_t       *rset;
    SOCKET      fd;
    int         i;
    slot_t      *sl;

    if ((rset = new_set()) == NULL)
        return 1;
    if (set != NULL)
    {
        for (i = 0; *n > 0 && i < set->s_nslots; ++i)
        {
            if ((sl = (slot_t *)&set->s_slots[i])->sl_key == NULL)
                continue;
            if (!isskt(sl->sl_key))
                continue;
            fd = sktof(sl->sl_key)->s_skt;
            if (FD_ISSET(fd, fds))
            {
                --*n;
                if (assign(rset, sktof(sl->sl_key), o_one))
                {
                    goto fail;
                }
            }
        }
    }
    if (assign(result, key, rset))
        goto fail;
    decref(rset);
    return 0;

fail:
    decref(rset);
    return 1;
}

/*
 * Check for ready sockets with optional timeout. This returns a
 * structure of three sets of descriptors. The parameters to
 * this function are complex. It takes two types of parameters,
 * an integer timeout value (in milliseconds) or up to to three
 * sets of socket descriptors (at least one set must be passed.)
 * The first set is taken to be the sockets to check for reading,
 * the second set is checked for writing and the third set is
 * checked for "urgent" status. The write and urgent sets are
 * optional. Any set may be specified as NULL. This is used to
 * specify, say, just a write set when there is no read set.
 * The read set must be specified (becauses the others are
 * optional) but there are no members.
 *
 * Aldem: dtabsize now is computed (as max found FD). It is more efficient
 * than select() on ALL FDs.
 */
static int
f_select(void)
{
    int                 i;
    int                 n;
    int                 dtabsize        = -1;
    long                timeout = -1;
    fd_set      fds[3];
    fd_set      *rfds = NULL;
    set_t               *rset = NULL;
    fd_set      *wfds = NULL;
    set_t               *wset = NULL;
    fd_set      *efds = NULL;
    set_t               *eset = NULL;
    struct timeval      timeval;
    struct timeval      *tv;
    struct_t            *result;
    set_t               *set  = NULL; /* Init. to remove compiler warning */
    int                 whichset = -1;  /* 0 == read, 1 == write, 2 == except*/
    slot_t              *sl;

    if (need_strings && define_strings())
        return 1;
    if (NARGS() == 0)
    {
        ici_error = "incorrect number of arguments for select()";
        return 1;
    }
    for (i = 0; i < NARGS(); ++i)
    {
        if (isint(ARG(i)))
        {
            if (timeout != -1)
            {
                ici_error = "more than one timeout parameter";
                return 1;
            }
            timeout = intof(ARG(i))->i_value;
            if (timeout < 0)
            {
                ici_error = "bad timeout";
                return 1;
            }
        }
        else if (isset(ARG(i)) || isnull(ARG(i)))
        {
            int j;

            if (++whichset > 2)
            {
                ici_error = "too many set/NULL parameters to select()";
                return 1;
            }
            if (isset(ARG(i)))
            {
                fd_set *fs = 0;

                switch (whichset)
                {
                case 0:
                    fs = rfds = &fds[0];
                    set = rset = setof(ARG(i));
                    break;
                case 1:
                    fs = wfds = &fds[1];
                    set = wset = setof(ARG(i));
                    break;
                case 2:
                    fs = efds = &fds[2];
                    set = eset = setof(ARG(i));
                    break;
                }
                FD_ZERO(fs);
                for (n = j = 0; j < set->s_nslots; ++j)
                {
                    int k;

                    if ((sl = (slot_t *)&set->s_slots[j])->sl_key == NULL)
                        continue;
                    if (!isskt(sl->sl_key))
                        continue;
                    if (isclosed(sktof(sl->sl_key)))
                    {
                        ici_error = "attempt to select on a closed socket";
                        return 1;
                    }
                    k = sktof(sl->sl_key)->s_skt;
                    FD_SET(k, fs);
                    if (k > dtabsize)
                        dtabsize = k;
                    ++n;
                }
                if (n == 0)
                {
                    switch (whichset)
                    {
                    case 0:
                        rfds = NULL;
                        rset = NULL;
                        break;
                    case 1:
                        wfds = NULL;
                        wset = NULL;
                        break;
                    case 2:
                        efds = NULL;
                        eset = NULL;
                        break;
                    }
                }
            }
        }
        else
        {
            return ici_argerror(i);
        }
    }
    if (rfds == NULL && wfds == NULL && efds == NULL)
    {
        ici_error = "nothing to select";
        return 1;
    }
    if (timeout == -1)
        tv = NULL;
    else
    {
        tv = &timeval;
        tv->tv_sec = timeout / 1000;
        tv->tv_usec = (timeout % 1000) * 1000;
    }
    if ((n = select(dtabsize + 1, rfds, wfds, efds, tv)) < 0)
        return serr("select");
#if 0
    if (n == 0)
    {
        ici_error = "select timeout";
        return 1;
    }
#endif
    if ((result = new_struct()) == NULL)
        return 1;
    /* Add in count */
    {
        int_t   *nobj;

        if ((nobj = new_int(n)) == NULL)
            goto fail;
        if (assign(result, string_n, nobj))
        {
            decref(nobj);
            goto fail;
        }
        decref(nobj);
    }
    if (select_add_result(result, string_read, rset, rfds, &n))
        goto fail;
    if (select_add_result(result, string_write, wset, wfds, &n))
        goto fail;
    if (select_add_result(result, string_except, eset, efds, &n))
        goto fail;
    return ici_ret_with_decref(objof(result));

fail:
    decref(objof(result));
    return 1;
}

/*
 * Send a message to a specific address.
 *
 * Usage
 *
 *      sendto(skt, msg, address)
 *
 */
static int
f_sendto(void)
{
    char                *addr;
    string_t            *msg;
    int                 n;
    skt_t               *skt;
    struct sockaddr_in  sockaddr;

    if (ici_typecheck("oos", &skt, &msg, &addr))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (!isstring(objof(msg)))
        return ici_argerror(1);
    if (parseaddr(addr, INADDR_LOOPBACK, &sockaddr) == NULL)
        return 1;
    if (isclosed(skt))
        return 1;
    n = sendto
    (
        skt->s_skt,
        msg->s_chars,
        msg->s_nchars,
        0,
        (struct sockaddr *)&sockaddr,
        sizeof sockaddr
    );
    if (n < 0)
        return serr("sendto");
    if (n != msg->s_nchars)
    {
        ici_error = "short write";
        return 1;
    }
    return null_ret();
}

#if 0
/*
 * Turn a textual send option into the correct bits
 */
static int
flagval(flag)
char *flag;
{
    if (!strcmp(flag, "oob"))
        return MSG_OOB;
    if (!strcmp(flag, "peek"))
        return MSG_PEEK;
    if (!strcmp(flag, "dontroute"))
        return MSG_DONTROUTE;
    return -1;
}
#endif

/*
 * Receive a message and get the source address. This returns a structure
 * (at the ICI level) containing the data, in result.msg, and the address
 * in result.addr.
 */
static int
f_recvfrom(void)
{
    skt_t               *skt;
    int                 len;
    int                 nb;
    char                *msg;
    struct sockaddr_in  addr;
    int                 addrsz = sizeof addr;
    struct_t            *result;
    string_t            *s;

    if (need_strings && define_strings())
        return 1;
    if (ici_typecheck("oi", &skt, &len))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if ((msg = ici_nalloc(len+1)) == NULL)
        return 1;
    if (isclosed(skt))
    {
        ici_free(msg);
        return 1;
    }
    if ((nb = recvfrom(skt->s_skt, msg, len, 0, (struct sockaddr *)&addr, &addrsz)) == -1)
    {
        ici_nfree(msg, len+1);
        return serr("recv");
    }
    if (nb == 0)
    {
        ici_nfree(msg, len+1);
        return null_ret();
    }
    if ((result = new_struct()) == NULL)
    {
        ici_nfree(msg, len+1);
        return 1;
    }
    if ((s = new_name(msg, nb)) == NULL)
    {
        ici_nfree(msg, len+1);
        return 1;
    }
    ici_nfree(msg, len+1);
    msg = NULL;
    if (assign(result, string_msg, s))
    {
        decref(s);
        goto fail;
    }
    decref(s);
    if ((s = new_cname(unparse_addr(&addr))) == NULL)
    {
        goto fail;
    }
    if (assign(result, string_addr, s))
    {
        decref(s);
        goto fail;
    }
    decref(s);
    return ici_ret_with_decref(objof(result));

fail:
    if (msg != NULL)
        ici_nfree(msg, len+1);
    decref(result);
    return 1;
}

/*
 * Send a message on a socket.
 */
static int
f_send(void)
{
    skt_t       *skt;
    string_t    *msg;

    if (ici_typecheck("oo", &skt, &msg))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (!isstring(objof(msg)))
        return ici_argerror(1);
    if (isclosed(skt))
        return 1;
    if (send(skt->s_skt, msg->s_chars, msg->s_nchars, 0) != msg->s_nchars)
    {
        ici_error = "short write";
        return 1;
    }
    return null_ret();
}

/*
 * string = recv(skt, size)
 *
 * Receive a message of up to size bytes on a socket and return a
 * string containing those bytes.
 */
static int
f_recv(void)
{
    skt_t       *skt;
    int         len;
    int         nb;
    char        *msg;
    string_t    *s;

    if (ici_typecheck("oi", &skt, &len))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (len <= 0)
        return ici_argerror(1);
    if ((msg = ici_alloc(len+1)) == NULL)
        return 1;
    if (isclosed(skt))
    {
        ici_nfree(msg, len+1);
        return 1;
    }
    if ((nb = recv(skt->s_skt, msg, len, 0)) == -1)
    {
        ici_nfree(msg, len+1);
        return serr("recv");
    }
    if (nb == 0)
    {
        ici_nfree(msg, len+1);
        return null_ret();
    }
    if ((s = new_name(msg, nb)) == NULL)
        return 1;
    ici_nfree(msg, len+1);
    return ici_ret_with_decref(objof(s));
}

/*
 * sockopt - turn a socket option name into an option code and level.
 *
 * Parameters:
 *      opt             The socket option, a string.
 *      level           A pointer to somewhere to store the
 *                      level parameter for {get,set}sockopt.
 *
 * Returns:
 *      The option code or -1 if no matching option found.
 */
static int
sockopt(char *opt, int *level)
{
    int code;
    int i;

    static struct
    {
        char    *name;
        int     value;
        int     level;
    }
    opts[] =
    {
        {"debug",       SO_DEBUG,       SOL_SOCKET},
        {"reuseaddr",   SO_REUSEADDR,   SOL_SOCKET},
        {"keepalive",   SO_KEEPALIVE,   SOL_SOCKET},
        {"dontroute",   SO_DONTROUTE,   SOL_SOCKET},
#ifndef __linux__
        {"useloopback", SO_USELOOPBACK, SOL_SOCKET},
#endif
        {"linger",      SO_LINGER,      SOL_SOCKET},
        {"broadcast",   SO_BROADCAST,   SOL_SOCKET},
        {"oobinline",   SO_OOBINLINE,   SOL_SOCKET},
        {"sndbuf",      SO_SNDBUF,      SOL_SOCKET},
        {"rcvbuf",      SO_RCVBUF,      SOL_SOCKET},
        {"type",        SO_TYPE,        SOL_SOCKET},
        {"error",       SO_ERROR,       SOL_SOCKET},

        {"nodelay",     TCP_NODELAY,    IPPROTO_TCP}

    };

    for (code = -1, i = 0; i < nels(opts); ++i)
    {
        if (!strcmp(opt, opts[i].name))
        {
            code = opts[i].value;
            *level = opts[i].level;
            break;
        }
    }
    return code;
}

/*
 * Get socket options.
 *
 * All option values get returned as integers. The only special processing
 * is of the "linger" option. This gets returned as the lingering time if
 * it is set or -1 if lingering is not enabled.
 */
static int
f_getsockopt(void)
{
    skt_t               *skt;
    char                *opt;
    int                 o;
    char                *optval;
    int                 optlen;
    int                 optlevel;
    struct linger       linger;
    int                 intvar;

    optval = (char *)&intvar;
    optlen = sizeof intvar;
    if (ici_typecheck("os", &skt, &opt))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    o = sockopt(opt, &optlevel);

    if (optlevel == SOL_SOCKET)
    {
        switch (o)
        {
        case SO_DEBUG:
        case SO_REUSEADDR:
        case SO_KEEPALIVE:
        case SO_DONTROUTE:
        case SO_BROADCAST:
        case SO_TYPE:
        case SO_OOBINLINE:
        case SO_SNDBUF:
        case SO_RCVBUF:
        case SO_ERROR:
            break;

        case SO_LINGER:
            optval = (char *)&linger;
            optlen = sizeof linger;
            break;

        default:
            goto bad;
        }
    }
    else if (optlevel == IPPROTO_TCP)
    {
        switch (o)
        {
        case TCP_NODELAY:
            break;

        default:
            goto bad;
        }
    }
    else
    {
        /* Shouldn't happen - sockopt returned a bogus level */
#ifndef NDEBUG
        abort();
#endif
        ici_error = "internal ici error in skt.c:sockopt()";
        return 1;
    }

    if (isclosed(skt))
        return 1;
    if (getsockopt(skt->s_skt, optlevel, o, optval, &optlen) == -1)
        return serr("getsockopt");
    if (o == SO_LINGER)
        intvar = linger.l_onoff ? linger.l_linger : -1;
    else
    {
        switch (o)
        {
        case SO_TYPE:
        case SO_SNDBUF:
        case SO_RCVBUF:
        case SO_ERROR:
            break;
        default:
            intvar = !!intvar;
        }
    }
    return int_ret(intvar);

bad:
    sprintf(buf, "bad socket option \"%s\"", opt);
    ici_error = buf;
    return 1;
}

/*
 * setsockopt(skt, string [, val])
 *
 * Set socket options.
 *
 * All socket options are integers. Again linger is a special case. The
 * option value is the linger time, if zero or negative lingering is
 * turned off.
 */
static int
f_setsockopt(void)
{
    skt_t               *skt;
    char                *opt;
    int                 optcode;
    char                *optval;
    int                 optlen;
    int                 optlevel;
    int                 intvar;
    struct linger       linger;

    if (ici_typecheck("os", &skt, &opt) == 0)
        intvar = 1; /* default to +ve action ... "set..." */
    else if (ici_typecheck("osi", &skt, &opt, &intvar))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    optcode = sockopt(opt, &optlevel);
    optval = (char *)&intvar;
    optlen = sizeof intvar;
    if (optlevel == SOL_SOCKET)
    {
        switch (optcode)
        {
        case SO_DEBUG:
        case SO_REUSEADDR:
        case SO_KEEPALIVE:
        case SO_DONTROUTE:
        case SO_BROADCAST:
        case SO_TYPE:
        case SO_OOBINLINE:
        case SO_SNDBUF:
        case SO_RCVBUF:
        case SO_ERROR:
            break;

        case SO_LINGER:
            linger.l_onoff = intvar > 0;
            linger.l_linger = intvar;
            optval = (char *)&linger;
            optlen = sizeof linger;
            break;

        default:
            goto bad;
        }
    }
    else if (optlevel == IPPROTO_TCP)
    {
        switch (optcode)
        {
        case TCP_NODELAY:
            break;

        default:
            goto bad;
        }
    }
    else
    {
        /* Shouldn't happen - sockopt returned a bogus level */
#ifndef NDEBUG
        abort();
#endif
        ici_error = "internal ici error in skt.c:sockopt()";
        return 1;
    }

    if (isclosed(skt))
        return 1;
    if (setsockopt(skt->s_skt, optlevel, optcode, optval, optlen) == -1)
        return serr("setsockopt");
    return ici_ret_no_decref(objof(skt));

bad:
    sprintf(buf, "bad socket option \"%s\"", opt);
    ici_error = buf;
    return 1;
}

/*
 * Get the host name as a string.
 */
static int
f_hostname(void)
{
    static string_t     *hostname = NULL;

    INITWINSOCK();
    if (hostname == NULL)
    {
        char name_buf[MAXHOSTNAMELEN];
        if (gethostname(name_buf, sizeof name_buf) == -1)
            return serr("gethostname");
        if ((hostname = new_cname(name_buf)) == NULL)
            return 1;
        incref(hostname);
    }
    return ici_ret_no_decref((object_t *)stringof(hostname));
}

/*
 * Return the name of the current user or the user with the given uid.
 */
static int
f_username(void)
{
    char        *s;
#ifdef _WIN32
    char        buffer[64];     /* I hope this is long enough! */
    int         len;

    INITWINSOCK();
    len = sizeof buffer;
    if (!GetUserName(buffer, &len))
        strcpy(buffer, "Windows User");
    s = buffer;
#else   /* #ifdef _WIN32 */
    /*
     * Do a password file lookup under Unix
     */
    char                *getenv();
    struct passwd       *pwent;
    long                uid = getuid();

    if (NARGS() > 0)
    {
        if (ici_typecheck("i", &uid))
            return 1;
    }
    if ((pwent = getpwuid(uid)) == NULL)
    {
        sprintf(buf, "can't find name for uid %ld", uid);
        ici_error = buf;
        return 1;
    }
    s = pwent->pw_name;
#endif
    return ici_ret_no_decref((object_t *)new_cname(s));
}

/*
 * Get the address of the connected socket's client.
 */
static int
f_getpeername(void)
{
    struct sockaddr_in  addr;
    int                 len = sizeof addr;
    skt_t               *skt;

    if (ici_typecheck("o", &skt))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (isclosed(skt))
        return 1;
    if (getpeername(skt->s_skt, (struct sockaddr *)&addr, &len) == -1)
        return serr("getpeername");
    return str_ret(unparse_addr(&addr));
}

/*
 * Get a socket's address.
 */
static int
f_getsockname(void)
{
    struct sockaddr_in  addr;
    int                 len = sizeof addr;
    skt_t               *skt;

    if (ici_typecheck("o", &skt))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (isclosed(skt))
        return 1;
    if (getsockname(skt->s_skt, (struct sockaddr *)&addr, &len) == -1)
        return serr("getsockname");
    return str_ret(unparse_addr(&addr));
}

/*
 * Get the port number bound to a socket.
 */
static int
f_getportno(void)
{
    struct sockaddr_in  addr;
    int                 len = sizeof addr;
    skt_t               *skt;

    if (ici_typecheck("o", &skt))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (isclosed(skt))
        return 1;
    if (getsockname(skt->s_skt, (struct sockaddr *)&addr, &len) == -1)
        return serr("getsockname");
    return int_ret(addr.sin_port);
}

/*
 * Return the IP address for the specified host. The address is returned
 * as a string containing the dotted decimal form of the host's address.
 * If the host's address cannot be resolved an error, "no such host"
 * is raised.
 */
static int
f_gethostbyname(void)
{
    char                *name;
    struct hostent      *hostent;
    struct in_addr      addr;

    INITWINSOCK();
    if (ici_typecheck("s", &name))
        return 1;
    if ((hostent = gethostbyname(name)) == NULL)
    {
        ici_error = "no such host";
        return 1;
    }
    memcpy(&addr, *hostent->h_addr_list, sizeof addr);
    return str_ret(inet_ntoa(addr));
}

/*
 * Return the name of a host given an IP address. The IP address is
 * specified as either a string containing an address in dotted
 * decimal or an integer containing the IP address in host byte
 * order (remember ICI ints are at least 32 bits so they can store
 * a 32 bit IP address).
 *
 * The name is returned as a string. If the name cannot be resolved
 * an exception, "unknown host", is raised.
 */
static int
f_gethostbyaddr(void)
{
    long                addr;
    char                *s;
    struct hostent      *hostent;

    INITWINSOCK();
    if (NARGS() != 1)
        return ici_argcount(1);
    if (isint(ARG(0)))
        addr = htonl((unsigned long)intof(ARG(0))->i_value);
    else if (ici_typecheck("s", &s))
        return 1;
    else if ((addr = inet_addr(s)) == 0xFFFFFFFF)
    {
        ici_error = "invalid IP address";
        return 1;
    }
    if ((hostent = gethostbyaddr((char *)&addr, sizeof addr, AF_INET)) == NULL)
    {
        ici_error = "unknown host";
        return 1;
    }
    return str_ret((char *)hostent->h_name);
}

/*
 * f_sktno - return the OS socket descriptor (file descriptor) for a socket
 */
static int
f_sktno(void)
{
    skt_t       *skt;

    if (ici_typecheck("o", &skt))
        return 1;
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (isclosed(skt))
        return 1;
    return int_ret((long)skt->s_skt);
}

/*
 * For turning sockets into files we implement a complete ICI ftype_t.
 * This is done because, (a) it works for all platforms and, (b) we
 * can control when the socket is closed (we don't close the underlying
 * socket when the file object is closed as occurs if fdopen() is used).
 */

enum
{
    SF_BUFSIZ   = 2048, /* Read ahead buffer size */
    SF_READ     = 1,    /* Open for reading */
    SF_WRITE    = 2,    /* Open for writing */
    SF_EOF      = 4     /* EOF read */
};

typedef struct
{
    skt_t       *sf_socket;
    char        sf_buf[SF_BUFSIZ];
    char        *sf_bufp;
    int         sf_nbuf;
    int         sf_pbchar;
    int         sf_flags;
}
skt_file_t;

static int
skt_getch(skt_file_t *sf)
{
    char    c;

    if (!(sf->sf_flags & SF_READ) || (sf->sf_flags & SF_EOF))
        return EOF;
    if (sf->sf_pbchar != EOF)
    {
        c = sf->sf_pbchar;
        sf->sf_pbchar = EOF;
    }
    else
    {
        if (sf->sf_nbuf == 0)
        {
            sf->sf_nbuf = recv(sf->sf_socket->s_skt, sf->sf_buf, SF_BUFSIZ, 0);
            if (sf->sf_nbuf <= 0)
            {
                sf->sf_flags |= SF_EOF;
                return EOF;
            }
            sf->sf_bufp = sf->sf_buf;
        }
        c = *sf->sf_bufp++;
        --sf->sf_nbuf;
    }
    return (unsigned char)c;
}

static int
skt_ungetc(int c, skt_file_t *sf)
{
    if (!(sf->sf_flags & SF_READ))
        return EOF;
    if (sf->sf_pbchar != EOF)
        return EOF;
    sf->sf_pbchar = c;
    return 0;
}

static int
skt_putch(int c, skt_file_t *sf)
{
    char    ch = c;

    if (!(sf->sf_flags & SF_WRITE))
        return EOF;
    for (;;)
    {
        switch (send(sf->sf_socket->s_skt, &ch, 1, 0))
        {
        case SOCKET_ERROR:
            return EOF;
        case 0:
            break;
        case 1:
            goto done;
        }
    }
done:
    return 0;
}

static int
skt_flush(skt_file_t *sf)
{
    return 0;
}

static int
skt_fclose(skt_file_t *sf)
{
    decref(sf->sf_socket);
    ici_tfree(sf, skt_file_t);
    return 0;
}

static long
skt_seek(void)
{
    ici_error = "cannot seek on a socket";
    return -1;
}

static int
skt_eof(skt_file_t *sf)
{
    return sf->sf_flags & SF_EOF;
}

static int
skt_write(char *buf, int n, skt_file_t *sf)
{
    int         nb;
    int         rc = 0;

    if (!(sf->sf_flags & SF_WRITE))
        return EOF;
    while (n > 0)
    {
        if ((nb = send(sf->sf_socket->s_skt, buf, n, 0)) == SOCKET_ERROR)
        {
            return EOF;
        }
        n -= nb;
        rc += nb;
        buf += nb;
    }
    return rc;
}

static ftype_t  skt_ftype =
{
    skt_getch,
    skt_ungetc,
    skt_putch,
    skt_flush,
    skt_fclose,
    skt_seek,
    skt_eof,
    skt_write
};

static skt_file_t *
skt_open(skt_t *s, char *mode)
{
    skt_file_t  *sf;

    if ((sf = ici_talloc(skt_file_t)) != NULL)
    {
        sf->sf_socket = s;
        sf->sf_pbchar = EOF;
        sf->sf_bufp = sf->sf_buf;
        sf->sf_nbuf = 0;
        incref(s);
        switch (*mode)
        {
        case 'r':
            sf->sf_flags = SF_READ;
            break;
        case 'w':
            sf->sf_flags = SF_WRITE;
            break;
        default:
            if (ici_chkbuf(strlen(mode) + 32))
                ici_error = "bad open mode for socket";
            else
            {
                sprintf(buf, "bad open mode, \"%s\", for socket", mode);
                ici_error = buf;
            }
            return NULL;
        }
    }
    return sf;
}

/*
 * f_sktopen - turn a socket descriptor into a file
 */
static int
f_sktopen(void)
{
    skt_t               *skt;
    char                *mode;
    file_t              *f;
    skt_file_t          *sf;

    if (ici_typecheck("os", &skt, &mode))
    {
        if (ici_typecheck("o", &skt))
            return 1;
        mode = "r";
    }
    if (!isskt(objof(skt)))
        return ici_argerror(0);
    if (isclosed(skt))
        return 1;
    if ((sf = skt_open(skt, mode)) == NULL)
        return 1;
    if ((f = new_file((char *)sf, &skt_ftype, NULL)) == NULL)
    {
        skt_fclose(sf);
        return 1;
    }
#ifdef NOTDEF
    if ((stream = fdopen(skt->s_skt, mode)) == NULL)
    {
        ici_error = "can't fdopen";
        return 1;
    }
    if ((f = new_file((char *)stream, &stdio_ftype, NULL)) == NULL)
    {
        fclose(stream);
        return 1;
    }
#endif
    return ici_ret_with_decref(objof(f));
}

#ifndef _WIN32
/*
 * f_socketpair - return a pair of connected stream sockets
 */
static int
f_socketpair(void)
{
    array_t     *a;
    skt_t       *s;
    int         sv[2];

    INITWINSOCK();
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1)
        return serr("socketpair");
    if ((a = new_array(2)) == NULL)
        goto fail1;
    if ((s = new_socket(sv[0])) == NULL)
    {
        decref(a);
        goto fail1;
    }
    *a->a_top++ = objof(s);
    decref(s);
    if ((s = new_socket(sv[1])) == NULL)
    {
        close(sv[1]);
        decref(a);
        goto fail;
    }
    *a->a_top++ = objof(s);
    decref(s);
    return ici_ret_with_decref(objof(a));

fail1:
    close(sv[0]);
    close(sv[1]);
fail:
    return 1;
}
#endif  /* #ifndef _WIN32 */

/*
 * Shutdown communications on a socket.
 *
 * socket = shutdown(socket [ , int ])
 *
 * Shutdown the send or receive or both sides of a TCP connection.
 * The optional int specifies which direction to shut down, 0 for
 * send, 1 for receive and 2 for both. The default is 2.
 *
 * Returns the socket.
 */
static int
f_shutdown(void)
{
    skt_t       *skt;
    long        flags;

    switch (NARGS())
    {
    case 1:
        if (ici_typecheck("o", &skt))
            return 1;
        flags = 2;
        break;

    case 2:
        if (ici_typecheck("oi", &skt, &flags))
            return 1;
        break;

    default:
        return ici_argcount(2);
    }
    shutdown(skt->s_skt, (int)flags);
    return ici_ret_no_decref(objof(skt));
}

/*
 * This is the configuration table that defines our functions to
 * the interpreter. The CF_OBJ macro comes from func.h and defines
 * the necessary object header for out cfunc_t table. The string
 * is the ICI name for the function. The last field is the address
 * of the C function that implements this function.
 */
cfunc_t skt_cfuncs[] =
{
    {CF_OBJ, "socket",          f_socket},
    {CF_OBJ, "listen",          f_listen},
    {CF_OBJ, "accept",          f_accept},
    {CF_OBJ, "connect",         f_connect},
    {CF_OBJ, "bind",            f_bind},
    {CF_OBJ, "select",          f_select},
    {CF_OBJ, "getsockopt",      f_getsockopt},
    {CF_OBJ, "setsockopt",      f_setsockopt},
    {CF_OBJ, "hostname",        f_hostname},
    {CF_OBJ, "username",        f_username},
    {CF_OBJ, "getpeername",     f_getpeername},
    {CF_OBJ, "getsockname",     f_getsockname},
    {CF_OBJ, "sendto",          f_sendto},
    {CF_OBJ, "recvfrom",        f_recvfrom},
    {CF_OBJ, "send",            f_send},
    {CF_OBJ, "recv",            f_recv},
    {CF_OBJ, "getportno",       f_getportno},
    {CF_OBJ, "gethostbyname",   f_gethostbyname},
    {CF_OBJ, "gethostbyaddr",   f_gethostbyaddr},
    {CF_OBJ, "sktno",           f_sktno},
    {CF_OBJ, "sktopen",         f_sktopen},
#ifndef _WIN32
    {CF_OBJ, "socketpair",      f_socketpair},
#endif
    {CF_OBJ, "shutdown",        f_shutdown},
    {CF_OBJ}
};

#endif /*NOSKT*/
