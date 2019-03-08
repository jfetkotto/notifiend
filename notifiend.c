/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/wireless.h>

#include <libnotify/notify.h>
#include <mpd/client.h>

#include "config.h"

static int running = 1;
static int tick = 0;
static char pnotification[200];
static char snotification[200];

void interrupt_handler(int);
int int_from_file(char *);
char char_from_file(char *);
void notify(NotifyUrgency);
void module_battery(void);
void module_wifi(void);
void module_mpd(void);

int
main(int argc, char *argv[])
{
    signal(SIGINT,interrupt_handler);

    while (running) {

        if (tick < 3) tick++;
        else tick = 0;

        module_battery();
        module_wifi();
        module_mpd();
        sleep(1);
    }
    return 0;
}

void
interrupt_handler(int sig)
{
    signal(SIGINT,SIG_IGN);
    running = 0;
    printf("\nExiting...\n");
}

int
int_from_file(char *file_name)
{
    int val = 0;
    FILE *file = fopen(file_name, "r");
    if (!file) {
        fprintf(stderr,"\nint_from_file => Error reading from file: %s\n", file_name);
        return -1;
    } else {
        fscanf(file,"%d", &val);
        fclose(file);
        return val;
    }
}

char
char_from_file(char *file_name)
{
    char val;
    FILE *file = fopen(file_name, "r");
    if (!file) {
        fprintf(stderr,"\nchar_from_file => Error reading from file: %s\n", file_name);
        return 'E';
    } else {
        fscanf(file,"%c",&val);
        fclose(file);
        return val;
    }
}

void
notify(NotifyUrgency urg)
{
    notify_init("notifiend_notification");
    NotifyNotification *notifiend_notification = notify_notification_new(pnotification,snotification,NULL);
    notify_notification_set_urgency(notifiend_notification,urg);
    notify_notification_show(notifiend_notification, NULL);
    g_object_unref(G_OBJECT(notifiend_notification));
    notify_uninit();

    sprintf(pnotification,"Empty");
    snotification[0] = 0;
}

void
module_battery(void)
{
    static int bat_charge;
    static char bat_state;
    int bat_charge_old = bat_charge;
    char bat_state_old = bat_state;


    if (tick == 0) {
        bat_charge = int_from_file(BAT_PERC_FILE);
        bat_state = char_from_file(BAT_STAT_FILE);
    }

    if (bat_charge >= BAT_FULL_CHARGE && bat_charge > bat_charge_old  && bat_state == 'C') {
        sprintf(pnotification,"Charged");
        sprintf(snotification,"Battery fully charged");
        notify(NOTIFY_URGENCY_NORMAL);
    }

    if (bat_charge <= BAT_EMPTY_CHARGE && bat_charge < bat_charge_old && bat_state == 'D') {
        sprintf(pnotification,"BATTERY LOW");
        sprintf(snotification,"Please plug in charger");
        notify(NOTIFY_URGENCY_CRITICAL);
    }

    if (bat_state == 'C'){
        if (bat_state_old == 'D' || bat_state_old == 'U') {
            sprintf(pnotification, "Charging");
            sprintf(snotification, "Charger has been plugged in");
            notify(NOTIFY_URGENCY_NORMAL);
        }
    }

    if (bat_state == 'D' && bat_state_old == 'C') {
        sprintf(pnotification, "Battery");
        sprintf(snotification, "Charger has been unplugged");
        notify(NOTIFY_URGENCY_NORMAL);
    }
}

void
module_wifi(void)
{
    static char essid[IW_ESSID_MAX_SIZE+1];
    static char essid_old[IW_ESSID_MAX_SIZE+1];
    struct iwreq req;
    int sockfd;
    char *id;
    char status[200];

    strcpy(req.ifr_ifrn.ifrn_name, IW_INTERFACE);
    if ((sockfd = socket(AF_INET,SOCK_DGRAM,0)) > -1) {
        id = calloc(IW_ESSID_MAX_SIZE+1,sizeof(char));
        req.u.essid.pointer = id;
        req.u.essid.length = IW_ESSID_MAX_SIZE;

        if (ioctl(sockfd,SIOCGIWESSID, &req) < 0) {
            fprintf(stderr, "\nFailed ESSID get on %s: %s\n",IW_INTERFACE,strerror(errno));
            close(sockfd);
        } else {
            strcpy(essid_old,essid);
            sprintf(essid,(char *)req.u.essid.pointer);

            if (strcmp(essid,"")) {
                if (strcmp(essid,essid_old)) {
                    sprintf(status,"Connected to %s", (char *)req.u.essid.pointer);
                    sprintf(pnotification,"WiFi");
                    sprintf(snotification,status);
                    notify(NOTIFY_URGENCY_NORMAL);
                }
            } else {
                if (strcmp(essid,essid_old)) {
                    sprintf(status, "Disconnected from WiFi");
                    sprintf(pnotification,"Wifi");
                    sprintf(snotification,status);
                    notify(NOTIFY_URGENCY_NORMAL);
                }
            }
        }
    close(sockfd);
    free(id);
    }
}

void
module_mpd(void)
{
    struct mpd_connection *conn;
    struct mpd_status *status;

    conn = mpd_connection_new(NULL,0,30000);

    static char artist[200] = "";
    static char title[200] = "";
    char artist_old[200] = "";
    char title_old[200] = "";

    strcpy(artist_old,artist);
    strcpy(title_old,title);

    struct mpd_song *song;
    mpd_command_list_begin(conn,true);
    mpd_send_status(conn);
    mpd_send_current_song(conn);
    mpd_command_list_end(conn);

    status = mpd_recv_status(conn);
    mpd_response_next(conn);
    if (tick == 0) {
        while ((song = mpd_recv_song(conn)) != NULL) {
            unsigned i = 0;
            const char *vart;
            const char *vtit;

            while((vart = mpd_song_get_tag(song,MPD_TAG_ARTIST,i++)) != NULL)
                sprintf(artist,"%s",vart);

            i = 0;

            while((vtit = mpd_song_get_tag(song,MPD_TAG_TITLE,i++)) != NULL)
                sprintf(title,"%s",vtit);

            mpd_song_free(song);
        }
    }


    if (strcmp(artist,"") && strcmp(title,"") && mpd_status_get_state(status) == MPD_STATE_PLAY) {
        if (strcmp(artist,artist_old) || strcmp(title,title_old)) {
            sprintf(pnotification,artist);
            sprintf(snotification,title);
            notify(NOTIFY_URGENCY_LOW);
        }
    }
    mpd_status_free(status);
    mpd_connection_free(conn);
}
