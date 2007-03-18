#include <sys/types.h>
#include <openssl/md5.h>
#include <stdlib.h>
#include <getopt.h>
#include "ntkresolv.h"

uint8_t mode_parsable_output= 0;
uint8_t mode_silent= 0;

void version(void)
{
    printf("ntk-resolv version %s (Netsukuku tools)\n\n"
        "Copyright (C) 2006.\n"
        "This is free software.  You may redistribute copies of it under the terms of\n"
        "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
        "There is NO WARRANTY, to the extent permitted by law.\n");
    exit(1);
}

void usage(void)
{
    printf("Usage:\n"
        "\tntk-resolv [OPTIONS] host\n"
        "\tntk-resolv -H host\n\n"
        "Options:\n"
        " -v --version          print version, then exit.\n"
        " -n --nameserver=ns    use nameserver `ns' instead of localhost.\n"
        " -P --port=port        nameserver port, default 53.\n"
        " -t --query-type=qt    query type (`-t help' shows more info).\n"
        " -r --realm=realm      realm to scan (`-r help' shows more info).\n"
        " -s --service=service  SNSD service (`-s help' shows more info).\n"
        " -p --protocolo=proto  SNSD protocol (`-p help' shows more info).\n"
        " -S --silent           ntk-resolv will be not loquacious.\n"
        " -b --block-recursion  set recursion OFF.\n"
        " -m --md5-hash         hostname specified is hash-ed.\n"
        " -H --compute-hash     print the hash'ed hostname.\n"
        " -l --parsable-output  print answers in a synthetic way.\n"
        " -h --help             display this help, then exit.\n\n");
    exit(1);
}
void qt_usage(char *arg)
{
    if (arg)
        printf("Bad Query Type %s\n\n",arg);
    else
        printf("ntk-resolv Query Type Help.\n\n");
    printf("Valid query types are:\n"
        " * snsd\t\thost:port -> ip\n"
        "   ptr\t\tip -> host\n"
        "   global\thostname -> all services ip\n"
        "   mx\t\thostname MX -> ip\n\n"
        "(you can also use univoque abbreviation)\n"
        "Note: mx query is equivalent to --query-type="
        "snsd AND --service=25\n\n");
    exit(1);
}
void realm_usage(char *arg)
{
    if (arg)
        printf("Bad Realm %s\n\n",arg);
    else
        printf("ntk-resolv Realm Help.\n\n");
    printf("Valid realms are:\n"
        " * ntk\tnetsukuku realm\n"
        "   inet\tinternet realm\n\n"
        "(you can also use univoque abbreviation)\n\n");
    exit(1);
}
void proto_usage(char *arg)
{
    if (arg)
        printf("Bad Protocol %s\n\n",arg);
    else
        printf("ntk-resolv Protocol Help.\n\n");
    printf("Valid protocols are:\n"
        " * tcp\n"
        "   udp\n"
        "(you can also use univoque abbreviation)\n"
        "Note: you can also specify the protocol with option `-s'.\n" 
        "To know more, type:\n"
        "\tntk-resolv -s help\n\n");
    exit(1);
}
void service_and_proto_usage(char *arg)
{
    if (arg)
        printf("Bad service/proto %s\n\n"
            "Use `ntk-resolv -s help` for more info on"
            " service and proto.\n" ,arg);
    else printf(
        "ntk-resolv Service and Proto Help.\n\n"
        "The form to specify a service and a protocol are:\n"
        "  ntk-resolv -s service/proto\n"
        "  ntk-resolv -s service -p proto\n\n"
        "Valid protocols are:\n"
        " * tcp\n"
        "   udp\n\n"
        "Valid services are expressed in /etc/services.\n"
        "You can use numeric form too.\n\n"
        "As example, the next commands are equivalent and\n"
        "will return the IP of the hostname that offers\n"
        "webpages for the hostname \"some_hostname\":\n\n"
        "  ntk-resolv -s http -p tcp some_hostname\n"
        "  ntk-resolv -s http/tcp    some_hostname\n"
        "  ntk-resolv -s 80/tcp      some_hostname\n"
        "  ntk-resolv -s 80          some_hostname\n\n");
    exit(1);
}
        

double diff_time(struct timeval a,struct timeval b)
{
    double res;
    res=(double)(b.tv_sec-a.tv_sec);
    if (res<0.9 || b.tv_usec>=a.tv_usec)
        res+=(b.tv_usec-a.tv_usec)/1000000.0;
    else {
        res-=1.0;
        res+=(1000000.0+b.tv_usec-a.tv_usec)/1000000.0;
    }
    return res;
}

void print_headers(andns_pkt *ap)
{
    printf("\n - Headers Section:\n"
           "\tid ~ %6d\tqr  ~ %4d\tqtype ~ %7s\n"
           "\tan ~ %6d\tipv ~ %s\trealm ~ %7s\n"
           "\tsv ~ %6s\tprt ~ %4s\trCode ~ %s\n"
           "\trc ~ %6d\n", 
           ap->id, ap->qr, QTYPE_STR(ap),
           ap->ancount, IPV_STR(ap), NK_STR(ap),
           SERVICE_STR(ap), PROTO_STR(ap),
           RCODE_STR(ap), ap->r);
}
void print_question(andns_query *q)
{
    printf("\n - Question Section:\n"
        "\tObj ~ %s\n\n", q->question);
}

void ip_bin_to_str(andns_pkt *ap, void *data,char *dst)
{
    int family;
    struct in_addr ia;
    struct in6_addr i6a;
    const void *via;
    const char *crow;

    family= ap->ipv== ANDNS_IPV4? AF_INET: AF_INET6;
    switch(family) 
    {
        case AF_INET:
            memcpy(&(ia.s_addr),data,4);
            via=(void*)(&ia);
            break;

        case AF_INET6:
            memcpy(&(i6a.in6_u),data,16);
            via=(void*)(&i6a);
            break;

        default:
            strcpy(dst,"Unprintable Object");
            return;
    }
    crow= inet_ntop(family, via, dst, ANDNS_MAX_NTK_HNAME_LEN);
    if (!crow) strcpy(dst,"Unprintable Object");
}

void answer_data_to_str(andns_pkt *ap, andns_pkt_data *apd, char *dst)
{
    if (ap->qtype==QTYPE_PTR) strcpy(dst, apd->rdata);
    else if (ap->qtype== QTYPE_G || ap->qtype== QTYPE_A) {
        if (apd->m & ANDNS_APD_IP) ip_bin_to_str(ap, apd->rdata, dst);
        else NTK_RESOLV_HASH_STR(apd->rdata, dst);
    } 
    else strcpy(dst,"Unprintable Object");
}

void print_answers(andns_query *q, andns_pkt *ap)
{
    int i=0;
    int ancount= ap->ancount;
    char temp[ANDNS_MAX_NTK_HNAME_LEN];
    andns_pkt_data *apd;

    if (!ancount)
        return;

    printf(" - Answers Section:\n");

    apd= ap->pkt_answ;
    while (apd) {
        i++;
        if (i> ancount) printf("Answer not declared in Headers Packet.\n");

        answer_data_to_str(ap, apd, temp);
        printf("\t ~ %s", temp);
        if (apd->m & ANDNS_APD_MAIN_IP) printf(" *");

        else if (ap->qtype!= QTYPE_PTR && !(apd->m & ANDNS_APD_IP) && ap->r)
            printf("\t + Recursion Failed");

        printf("\n");

        if (ap->qtype== QTYPE_A || ap->qtype== QTYPE_G) 
            printf("\t\tPrio ~ %d  Weigth ~ %d\n", apd->prio, apd->wg);

        if (ap->qtype==QTYPE_G)
            printf("\t\tService ~ %d  Proto ~ %s\n", 
                apd->service, apd->m & ANDNS_APD_UDP? "udp":"tcp");

        printf("\n");

        apd= apd->next;
    }
}

void print_parsable_answers(andns_query *q, andns_pkt *ap)
{
    int i=0;
    int ancount= ap->ancount;
    char temp[ANDNS_MAX_NTK_HNAME_LEN];
    andns_pkt_data *apd;

    if (!ancount)
        return;

    apd= ap->pkt_answ;
    while(apd) {
        i++;
        if (i>ancount) printf("Answer not declared in Headers Packet.\n");

        answer_data_to_str(ap, apd, temp);

        if (ap->qtype==QTYPE_PTR || (ap->qtype==QTYPE_A && !ap->service)) 
            printf("%s %s\n", NTK_RESOLV_SYMBOL(apd), temp);

        else if (ap->qtype== QTYPE_A) 
            printf("%s %s %d %d\n",NTK_RESOLV_SYMBOL(apd),
                temp, apd->prio, apd->wg);

        else 
            printf("%s %s %d %s %d %d\n", NTK_RESOLV_SYMBOL(apd),
                temp, apd->service, apd->m&ANDNS_APD_UDP?"udp":"tcp",
                apd->prio,apd->wg);

        apd=apd->next;
    }
}

void print_results(andns_query *q, andns_pkt *ap) 
{
    if (!mode_silent) {
        print_headers(ap);
        print_question(q);
    }

    if (mode_parsable_output) print_parsable_answers(q, ap);
    else print_answers(q, ap);
}

void do_command(andns_query *q, const char *qst)
{
    char buf[ANDNS_PKT_QUERY_SZ];
    int res;
    andns_pkt *ap;

    if (strlen(qst)>= ANDNS_MAX_NTK_HNAME_LEN) {
        printf("Question is too long.");
        exit(1);
    }
        
    strcpy(q->question, qst);

    ap= ntk_query(q);
    if (!ap) {
        printf("Error: %s.", q->errors);
        exit(1);
    }

    print_results(q, ap);
    destroy_andns_pkt(ap);
}

void opts_init(andns_query *q)
{
    q->hashed=          0;
    q->andns_server=    0;
    q->port=            0;
    q->service=         0;
    q->proto=           0;
    q->recursion=       1;
    q->type=            QTYPE_A;
    q->realm=           ANDNS_NTK_REALM;
}

void opts_set_ns(andns_query *q, const char *h)
{
    struct sockaddr_in saddr;
    struct sockaddr_in6 saddr6;
    int res;

    res= inet_pton(AF_INET, h, &saddr);
    if (res<= 0) {
        res= inet_pton(AF_INET6, h, &saddr6);
        if (res<= 0) {
            printf("Invalid andns server %s.\n", h);
            exit(1);
        }
    }

    if (!(q->andns_server= (char*)malloc(1 + strlen(h)))) {
        printf("Memory Allocation problem. Abort.");
        exit(1);
    }
    strcpy(q->andns_server, h);
}

void opts_set_qt(andns_query *q, char *arg)
{
    int res;

    if (!strcmp(arg, HELP_STR)) qt_usage(NULL);

    res= QTFROMPREF(arg);
    if (res==-1) qt_usage(arg);

    q->type= res;
    if (res== QTYPE_MX) {
        q->service= 25;
        q->proto= ANDNS_PROTO_TCP;
    } 
}

void opts_set_realm(andns_query *q, char *arg)
{
    uint8_t res;

    if (!strcmp(arg, HELP_STR)) realm_usage(NULL);
    res= REALMFROMPREF(arg);
    if (!res) realm_usage(arg);
    q->realm= res;
}

void opts_set_service_and_proto(andns_query *q, char *arg)
{
    int ret;
    char *proto;
    struct servent *st;

    if (!strcmp(arg, HELP_STR)) service_and_proto_usage(NULL);

    proto= strchr(arg, '/');
    if (proto) {
        *proto++= 0;
        if (strcmp(proto, "tcp")) q->proto= 0;
        else if (strcmp(proto, "udp")) q->proto= 1;
        else {
            printf("Invalid Protocol `%s`.\n", proto);
            exit(1);
        }
    }
    
    if (!isdigit(arg[0])) {
        if(!(st= getservbyname(arg, 0))) {
            printf("Invalid Service `%s`.\n", arg);
            exit(1);
        }
        q->service= ntohs(st->s_port);
    } 
    else q->service= atoi(arg);
}

void opts_set_proto(andns_query *q, char *arg) 
{
    int ret;

    if (!strcmp(arg,HELP_STR)) proto_usage(NULL);
    ret= PROTOFROMPREF(arg);
    if (ret<0) proto_usage(arg);
    q->proto= ret;
}

void compute_hash(const char *arg)
{
    char hash[17], temp[16];

    MD5(arg, strlen(arg), temp);
    NTK_RESOLV_HASH_STR(temp, hash);
    hash[16]=0;
    printf("%s\n", hash);
    exit(0);
}

int main(int argc, char **argv)
{
    int c, mode_compute_hash=0;
    extern int optind, opterr, optopt;
    extern char *optarg;
    static struct timeval time_start, time_stop;
    andns_query query, *q;

    static struct option longopts[]= {
                {"version",0,0,'v'},
                {"nameserver",1,0,'n'},
                {"port",1,0,'P'},
                {"query-type",1,0,'t'},
                {"realm",1,0,'r'},
                {"service",1,0,'s'},
                {"proto",1,0,'p'},
                {"silent",0,0,'S'},
                {"block-recursion",0,0,'b'},
                {"md5-hash",0,0,'m'},
                {"compute-hash",0,0,'H'},
                {"parsable-output",0,0,'l'},
                {"help",0,0,'h'},
                {0,0,0,0}
        };

    q= &query;
    opts_init(q);

    while(1) {

        c= getopt_long(argc, argv, "vn:P:t:r:s:p:ShbmHl", longopts, 0);
        if (c==-1)
            break;

        switch(c) {
            case 'v':
                version();
                break;
            case 'n':
                opts_set_ns(q, optarg);
                break;
            case 'P':
                q->port= atoi(optarg);
                break;
            case 't':
                opts_set_qt(q, optarg);
                break;
            case 'r':
                opts_set_realm(q, optarg);
                break;
            case 's':
                opts_set_service_and_proto(q, optarg);
                break;  
            case 'p':
                opts_set_proto(q, optarg);
                break;  
            case 'h':
                usage();
                break;
            case 'S':
                mode_silent= 1;
                break;
            case 'b':
                q->recursion= 0;
                break;
            case 'm':
                q->hashed= 1;
                break;
            case 'H':
                mode_compute_hash= 1;
                break;
            case 'l':
                mode_parsable_output= 1;
                break;
            default:
                usage();
                break;
        }
    }

    if (optind == argc) usage();
    if (mode_compute_hash) compute_hash(argv[optind]);

    if (!mode_silent) gettimeofday(&time_start, NULL);
    do_command(q, argv[optind]);
    
    if (!mode_silent) {
        gettimeofday(&time_stop, NULL);
        printf("Query time: %f seconds.\n", diff_time(time_start, time_stop));
        printf("\tBye.\n");
    }

    return 0;
}
