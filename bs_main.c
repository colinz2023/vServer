#include "core/dev_event_timer.h"
#include "core/dev_event.h"
#include "bs_main.h"
#include "bs_utils.h"
#include "bs_vserver.h"
#include "bs_log.h"
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <signal.h>


dev_event_loop_t *BsLoop = NULL;
dev_event_t *BsTimer = NULL;
dev_event_t *BsVserver = NULL;
dev_event_t *BsCmd = NULL;

static int log_flag = 0;

static void sys_quit(void);

static char *
skip_space(const char *p)
{
    while (isspace(*p)) p++;
    return (char *)p;
}

char *
check_dot(char *p)
{
    while (*p != '.') 
    {
        if (!isdigit(*p)) {
            *p = 0;
            return p;
        }
        p++;
    }
    p++;
    return (char *)p;
}

int 
_ip_addr_add(char *str, struct vbs_instance_array *intances)
{
    int dot_num = 0;
    int a, b, c, d;
    char *p = str;
    int n = 0;
    int i = 0;
    int stat = 0;
    char ip[32];

    p = check_dot(p);
    while (*p != 0) {
        p = check_dot(p);
        dot_num++;
    }

    if (dot_num != 2) {
        return -1;
    }

    sscanf((const char *)str, "%d.%d.%d", &a, &b, &c);
    if((a < 1 || a > 255) ||
        (b < 1 || b > 255) ||
        (c < 1 || c > 255)) {
        return -1;
    }

    p = p+1;
    while (*p != 0 && n < 3) {
        int xx = atoi(p);
        if (xx == 1) {
            stat = stat | (1 << (n));
        }
        n++;
    }
    for (i = 1; i < 255; i++) {
        vbs_instance_t vit;
        memset(&vit, 0, sizeof(vit));
        snprintf(ip, sizeof(ip), "%d.%d.%d.%d", a, b, c, i);
        console_print("adding %s, stat = %d\n", ip, stat);
        vit.stat = stat;
        vit.cpu_rate = i % 100;
        vit.mem_total =  4096 + 256*256 + 64*256;
        vit.mem_cache = a + b + c + 128*i;
        vit.mem_buffer = a + b + c + 64*i;
        vit.mem_free = a + b + c + 128*i;
        inet_pton(AF_INET, ip, &vit.ipaddr);
        addr_add(intances, &vit);
    }

    return 0;
}


void help()
{
    console_print("command: \n");
    console_print("get ip_address [get back server parameter and statstics] \n"
                  "add ip_address parameter [add back server] \n"
                  "showall [show all back server] \n"
                  "clear [clear screen] \n"
                  "exit [exit program] \n\n");
}

int 
cmd_input_hander(void *data)
{
    struct vbs_instance_array *intances = (struct vbs_instance_array *)data;
    vbs_instance_t *vbs_inst;
    char buffer[512];
    int idx, len;
    char *p;
    
    memset(buffer, 0, sizeof(buffer));
    int buff_len = get_io_buff_len(STDIN_FILENO);
    if (buff_len > sizeof(buffer)) buff_len = sizeof(buffer);

    len = read(STDIN_FILENO, buffer, buff_len);
    if (len <= 3) {
        console_print("$");
        return 0;
    }
    buffer[len - 1] = 0;
    
    if (strncmp(buffer, "get", 3) == 0) {
        p = strtok(buffer+3, " ");
        if (p == NULL) { console_print("$"); return 0;}

        idx = addr_search(intances, p);
        if (idx < 0) {
            console_print("Can not find %s , idx = %d\n", p, idx);
        } else {
            vbs_inst = &intances->array[idx];
            console_print(CONSOLE_BACK_N(1));
            instance_print(vbs_inst, 1);
        }
    } else if (strncmp(buffer, "set", 3) == 0) {
        p = strtok(buffer+3, " ");
        if (p == NULL) {console_print("$"); return 0;}
        char *para = strtok(NULL, " ");
        if (para == NULL) {console_print("$"); return 0;}
        idx = addr_search(intances, p);
        if (idx < 0) {
            console_print("Can not find %s , idx = %d\n", p, idx);
        } else {
            vbs_inst = &intances->array[idx];
            int stat = 0;
            int n = 0;

            while (*para != 0 && n < 3) {
                int xx = atoi(para);
                if (xx == 1) {
                    stat = stat | (1 << (n));
                }
                n++;
                para = strtok(NULL, " ");
                if (para == NULL)break;
            }
            vbs_inst->stat = stat;
            console_print(CONSOLE_BACK_N(1));
            instance_print(vbs_inst, 1);
        }
    } else if (strncmp(buffer, "add", 3) == 0) {
        p = skip_space(buffer+3);
        int ret = _ip_addr_add(p, intances);
        if (ret < 0) {
            console_print("%s\n", "error");
        }
    } else if (strncmp(buffer, "clear", 5) == 0) {
        console_print(CONSOLE_CLEAR);
    }  else if (strncmp(buffer, "showall", 7) == 0  || strncmp(buffer, "show", 4) == 0) {
        addr_print(intances);
    }
     else if (strncmp(buffer, "help", 4) == 0) {
        console_print(CONSOLE_CLEAR);
        help();
    }  else if (strncmp(buffer, "exit", 4) == 0) {
        exit(0);
    } else if (strncmp(buffer, "quit", 4) == 0) {
        sys_quit();
    } else {
        console_print(CONSOLE_CLEAR);
    }

    console_print("$");
    return 0;
}

#define WELCOME_STR \
"####################################################################\n"\
"#      Back sever agent:  V1.0.1 , 2016.12.30                      #\n"\
"#------------------------------------------------------------------#\n"\
"#      Welcome! Start to use the command line!                     #\n"\
"####################################################################\n"
void printf_banner(void)
{
    console_print(CONSOLE_CLEAR);
    console_print("%s", WELCOME_STR);
    help();
    console_print("$");
}

dev_event_t * 
bs_cmd_creat(void *data)
{
    dev_event_t* vs;
    //set_nonblocking(STDIN_FILENO);
    vs = dev_event_creat(STDIN_FILENO, EPOLLIN, cmd_input_hander, (void *)data, 0);
    printf_banner();
    return vs;
}


int 
ev_loop_cb(void *data, uint32_t events)
{
    dev_event_t *ev = (dev_event_t *)data;
    //printf("events =%d\n\n", events);
    if (ev == BsTimer) {
        ev->handler(data);
    } else {
        ev->handler(dev_event_get_data(ev));
    }
    return 0;
}

void signal_init()
{
    struct sigaction act;
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGQUIT);
    sigaddset(&act.sa_mask, SIGTERM);
    sigaddset(&act.sa_mask, SIGINT);
    // act.sa_flags = SA_RESETHAND;
    // act.sa_flags = SA_NODEFER;
    act.sa_flags = 0;

    sigaction(SIGINT, &act, 0);
}

static int 
sys_init(void)
{
    signal_init();
    nice(-19);
    log_open(LOG_PATH, log_flag);
    return 0;
}

static void 
sys_quit(void)
{
    log_close();
    dev_event_timer_destory(BsTimer);
    dev_event_loop_destory(BsLoop);
    bs_vserver_free(BsVserver);
    exit(0);
}

int main(int argc, char *argv[])
{
    int ret = 0, opt;
    int opt_index = 0;
    char *ifn = NULL, *ip_list = NULL;
   
    struct option long_opts[] = {
        {"interface", required_argument, 0, 'i'},
        {"ip_list", required_argument, 0, 'l'},
        {"log", 0, 0, 'L'},
    };

    if (lock_file_init("bsd")) {
        printf("%s\n", "error ocuur or one program is running");
        exit(-1);
    } 

    while ((opt = getopt_long(argc, argv, "i:l:L", long_opts, &opt_index)) != -1) {
        switch (opt) {
            case 'i':
                ifn = optarg;
                break;            
            case 'l':
                ip_list = optarg;
                break;
            case 'L':
                log_flag = 1;
                break;
        }
    }

    if (ifn == NULL) ifn = "eth0";
    if (ip_list == NULL) ip_list = "./ip_list";

    if (if_not_exist(ifn)) {
        fprintf(stderr, "interface %s do not exit\n", ifn);
        exit(-1);
    }

    sys_init();

    BsLoop = dev_event_loop_creat(4, ev_loop_cb);
    BsTimer = dev_event_timer_creat(8, NULL);
    if (BsTimer == NULL || BsLoop == NULL) {
        fprintf(stderr, "%s\n", "error\n");
        exit(-1);
    }

    struct bs_global * bsg = calloc(1, sizeof(struct bs_global));
    snprintf(bsg->ifn, sizeof(bsg->ifn), "%s", ifn);
    snprintf(bsg->ip_list_path, sizeof(bsg->ip_list_path), "%s", ip_list);

    BsVserver = bs_vserver_creat(bsg);
    if (BsVserver == NULL) {
        fprintf(stderr, "%s\n", "creat vserver fail");
        exit(-1);
    }

    BsCmd = bs_cmd_creat(bsg->vbs_instances);

    dev_event_loop_add(BsLoop, BsTimer);
    dev_event_loop_add(BsLoop, BsVserver);
    dev_event_loop_add(BsLoop, BsCmd);
    dev_event_loop_run(BsLoop);
    return 0;
}

