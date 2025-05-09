#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <libudev.h>
#include <libinput.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <SDL2/SDL.h>
#include <poll.h>
#include <time.h>

#define CONFIG_PATH_FMT "%s/.config/lscreensaver/config"
#define MAX_EXEMPTS 32
#define MAX_LINE 256

static int inactivity_time = 600;      // seconds
static char *exempts[MAX_EXEMPTS];
static int exempt_count = 0;

static struct libinput *li = NULL;
static SDL_Window *win = NULL;
static bool blanked = false;
static time_t blank_time = 0;

// simple ini loader
static void load_config(){
    char path[512];
    snprintf(path, sizeof(path), CONFIG_PATH_FMT, getenv("HOME"));
    FILE *f = fopen(path,"r");
    if(!f) return;
    char line[MAX_LINE];
    while(fgets(line,sizeof(line),f)){
        char *p=line;
        while(*p==' '||*p=='\t')p++;
        if(*p=='\0'||*p=='#') continue;
        char *eq = strchr(p,'=');
        if(!eq) continue;
        *eq=0; char *key=p, *val=eq+1;
        // strip newline
        char *nl = strchr(val,'\n'); if(nl)*nl=0;
        if(strcmp(key,"inactivity_time")==0){
            inactivity_time = atoi(val);
        } else if(strcmp(key,"exempt_processes")==0){
            char *tok=strtok(val,",");
            while(tok && exempt_count<MAX_EXEMPTS){
                exempts[exempt_count++] = strdup(tok);
                tok = strtok(NULL,",");
            }
        }
    }
    fclose(f);
}

// returns true if any process name matches
static bool any_exempt_running(){
    DIR *pd = opendir("/proc");
    if(!pd) return false;
    struct dirent *de;
    while((de=readdir(pd))){
        if(de->d_type!=DT_DIR) continue;
        pid_t pid = atoi(de->d_name);
        if(pid<=0) continue;
        char cmdpath[64], buf[256];
        snprintf(cmdpath,sizeof(cmdpath),"/proc/%d/comm",pid);
        FILE *c=fopen(cmdpath,"r");
        if(!c) continue;
        if(fgets(buf,sizeof(buf),c)){
            // strip newline
            char *nl=strchr(buf,'\n'); if(nl)*nl=0;
            for(int i=0;i<exempt_count;i++){
                if(strcmp(buf,exempts[i])==0){
                    fclose(c);
                    closedir(pd);
                    return true;
                }
            }
        }
        fclose(c);
    }
    closedir(pd);
    return false;
}

// libinput callbacks
static int open_restricted(const char *path, int flags, void *user_data){
    return open(path, flags);
}
static void close_restricted(int fd, void *user_data){
    close(fd);
}

static const struct libinput_interface li_if = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

static void init_libinput(){
    struct udev *udev = udev_new();
    if(!udev) exit(1);
    li = libinput_udev_create_context(&li_if, NULL, udev);
    libinput_udev_assign_seat(li, "seat0");
}

static void create_blank_window(){
    if(win) return;
    SDL_ShowCursor(SDL_DISABLE);
    win = SDL_CreateWindow("blank",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        0,0, SDL_WINDOW_FULLSCREEN_DESKTOP | SDL_WINDOW_BORDERLESS );
    if(!win) { fprintf(stderr,"SDL_CreateWindow: %s\n", SDL_GetError()); return; }
    SDL_Renderer *r = SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawColor(r,0,0,0,255);
    SDL_RenderClear(r);
    SDL_RenderPresent(r);
    SDL_DestroyRenderer(r);
    blanked = true;
    blank_time = time(NULL);
}

static void destroy_blank_window(){
    if(!win) return;
    SDL_DestroyWindow(win);
    win=NULL;
    SDL_ShowCursor(SDL_ENABLE);
    blanked = false;
    blank_time = 0;
}

int main(){
    load_config();
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){
        fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());
        return 1;
    }
    init_libinput();
    int li_fd = libinput_get_fd(li);

    struct pollfd fds[2];
    fds[0].fd = li_fd; fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO; fds[1].events = 0; // placeholder if needed

    time_t last_activity = time(NULL);

    while(1){
        // compute timeout until next event or inactivity trigger
        int timeout_ms = 1000; // wake every second to check
        poll(fds,1, timeout_ms);

        // handle libinput events
        libinput_dispatch(li);
        struct libinput_event *ev;
        while((ev = libinput_get_event(li))){
            enum libinput_event_type t = libinput_event_get_type(ev);
            if(t==LIBINPUT_EVENT_POINTER_MOTION ||
               t==LIBINPUT_EVENT_KEYBOARD_KEY ||
               t==LIBINPUT_EVENT_POINTER_BUTTON ||
               t==LIBINPUT_EVENT_TOUCH_DOWN){
                time(&last_activity);
                if(blanked && blank_time>0){
                    // only unblank if grace period passed
                    if(time(NULL) - blank_time >= 3){
                        destroy_blank_window();
                    }
                }
            }
            libinput_event_destroy(ev);
        }

        // check for blank condition
        if(!blanked){
            time_t now = time(NULL);
            if( (now - last_activity) >= inactivity_time ){
                if(!any_exempt_running()){
                    create_blank_window();
                } else {
                    // reset timer if exempt app is running
                    time(&last_activity);
                }
            }
        }
    }

    SDL_Quit();
    return 0;
}