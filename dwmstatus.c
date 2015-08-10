#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>


#include <X11/Xlib.h>

char *tzcentral = "America/Rainy_River";
int old_stats[10];

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	memset(buf, 0, sizeof(buf));
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		perror("getloadavg");
		exit(1);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *
cpu_usage(void)
{
        FILE *f;
        int i = 0, stats[10], delta[10];
        f = fopen("/proc/stat", "r");
        if(f == NULL)
            return smprintf("ERROR");
        
        /* Get current stats*/
        fscanf(f, "%*s");
        for(i = 0; i < 10; ++i)
        {   
            char str[10];
            fscanf(f, "%s", str);
            stats[i] = atoi(str);
            
            /* Compare it with old_stats to create delta */
            delta[i] = stats[i] - old_stats[i];
            /* Create new old_stats */
            old_stats[i] = stats[i];
        }
        int total_used = delta[0] + delta[1] + delta[2] + delta[4] + delta[5] + delta[6] + delta[7] + delta[8] + delta[9];
        int total = total_used + delta[3];
        fclose(f);
        return smprintf("%2.0f%%", (float)total_used*100/total);
}

char *
getbattery(void)
{
	FILE* f;
	char bat[5];
	f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
	fscanf(f, "%s", bat);
	fclose(f);
    	return smprintf("%s", bat);
}

char *
getifaddr(void)
{
	struct ifaddrs *ifaddr, *ifa;
        char host[NI_MAXHOST], output[200] = { 0 }, buf[100];

        if(getifaddrs(&ifaddr) == 1)    
                return 0;

        for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
        {       
                if(strncmp(ifa->ifa_name, "virbr", 5) != 0 && strncmp(ifa->ifa_name, "lo", 2) != 0 && ifa->ifa_addr->sa_family == AF_INET )
                {
                        getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                        sprintf(buf, " %s: %s |", ifa->ifa_name, host);
                        strcat(output, buf);
                }
        }
        freeifaddrs(ifaddr);
        return smprintf("%s", output);
}

char *
gettemp(void)
{
	FILE* thermal_zone0 = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
        FILE* thermal_zone1 = fopen("/sys/class/thermal/thermal_zone1/temp", "r");

        float temp0, temp1;
        temp0 = temp1 = 0;

        fscanf(thermal_zone0, "%f", &temp0);
        fscanf(thermal_zone1, "%f", &temp1);
	fclose(thermal_zone0);
	fclose(thermal_zone1);
        temp0 /= 1000;
        temp1 /= 1000;

        return smprintf("TEMP %.1fC %.1fC", temp0, temp1);
}

int
main(void)
{
	char *status;
	/*char *avgs;*/
	char *tmcentral;
        char *cpu;
	char *battery;
	char *ifaddr;
	char *temp;
        int i = 0;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

        /* Clear old_stats for cpu usage*/
        for(i = 0; i < 10; ++i)
            old_stats[i] = 0;

	for (;;sleep(1)) {
		/*avgs = loadavg();*/
                cpu = cpu_usage();
		tmcentral = mktimes("%Y-%m-%d %H:%M:%S", tzcentral);
		battery = getbattery();
		ifaddr = getifaddr();
		temp = gettemp();

		status = smprintf("%s | %s BAT %s | CPU %s | %s",
				temp, ifaddr, battery, cpu, tmcentral);
		setstatus(status);
		/*free(avgs);*/
		free(tmcentral);
		free(status);
		free(ifaddr);
	}

	XCloseDisplay(dpy);

	return 0;
}

