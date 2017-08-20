/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#include "FileSystem.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <linux/limits.h>
#include <linux/wireless.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

#include "wirelesstools.h"


using namespace netflix;
using namespace netflix::device;

/* Fetch the MACAddress of a network interface */
static std::string getMACAddress(char* ifname)
{
    struct ifreq ifr;
    int sock;
    unsigned char chMAC[6];

    sock=socket(AF_INET,SOCK_DGRAM,0);
    strcpy( ifr.ifr_name, ifname );
    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl( sock, SIOCGIFHWADDR, &ifr ) < 0) {
        close(sock);
        return std::string();
    }
    memcpy(chMAC, ifr.ifr_hwaddr.sa_data, 6);
    char str[100];
    for(int i=0;i<6;i++)
        sprintf(str,"%02X:%02X:%02X:%02X:%02X:%02X",chMAC[0],chMAC[1],chMAC[2],chMAC[3],chMAC[4],chMAC[5]);
    close(sock);

    return std::string(str);
}

/* Fetch the IP Address of a network interface*/
static std::string getIPAddress(std::string nif)
{
    int fd;
    struct ifreq ifr;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, nif.c_str(), IFNAMSIZ-1);
    ifr.ifr_name[IFNAMSIZ-1] = '\0';
    if(ioctl(fd, SIOCGIFADDR, &ifr) != 0)
    {
        close(fd);
        return std::string("");
    }
    close(fd);
    char str[INET_ADDRSTRLEN];
    if(inet_ntop(AF_INET, &((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr, str, INET_ADDRSTRLEN))
        return std::string(str);
    return std::string("");
}

static std::vector<std::string> getIPv6Address(char* nif)
{
    sockaddr_in6 sin6;

    std::vector<std::string> v;
    char IPV6_RESOURCE_FILE[]="/proc/net/if_inet6";
    if(FILE *fp = fopen(IPV6_RESOURCE_FILE, "r")) {
        char address[8][5];
        unsigned int if_idx = 0, plen = 0, scope = 0, dad_status = 0;
        char iface[33] = {0};
        unsigned int tmp = 0;
        memset((char *)address, 0x00, sizeof(address));
        while (EOF != fscanf(fp, "%4s%4s%4s%4s%4s%4s%4s%4s %02x %02x %02x %02x %20s\n",
                             address[0], address[1], address[2], address[3],
                             address[4], address[5], address[6], address[7],
                             &if_idx, &plen, &scope, &dad_status, iface)) {
            if(!strcmp(nif,iface)) {
                for (int i = 0; i < 8; i++) {
                    sscanf(address[i], "%4x", &tmp);
                    sin6.sin6_addr.s6_addr16[i] = htons((unsigned short int)tmp);
                }

                char name[128] = {0};
                char fname[128] = {0};
                if(inet_ntop(AF_INET6, &sin6.sin6_addr, name, sizeof(name)))
                {
                    sprintf(fname,"%s/%d", name, plen);
                    v.push_back(fname);
                } else
                    v.push_back("");
            }
        }
        fclose(fp);
    }
    return v;
}

static inline bool hasRoute(const char *nf, int flag, bool isIPv6 )
{
    bool result = false;
    if(!isIPv6){
        const char* fileName = "/proc/net/route";
        int r;
        if (FILE* fp = fopen(fileName, "r")) {
            char line[1024];
            int nfBegin = 0, nfEnd = 0, nfFlag = 0;
            const int nfLen = strlen(nf);
            while (!feof(fp)) {
                if (!fgets(line, sizeof(line), fp))
                    break;
                r = sscanf(line, "%n%*s%n %*s %*s %d %*s %*s %*s %*s %*s %*s %*s", &nfBegin, &nfEnd, &nfFlag);
                if (r == 1 && nfBegin == 0 && nfEnd == nfLen && !strncmp(nf, line, nfLen) && (flag == -1 || nfFlag == flag)) {
                    result = true;
                    break;
                }
            }
            fclose(fp);
        }
    } else {
        std::string interface(nf);
        std::string line = "";
        std::ifstream infile;
        infile.open("/proc/net/ipv6_route");
        do{
            getline(infile, line);
            //printf("line %s\n", line.c_str());
            std::istringstream iss(line);
            std::vector<std::string> token;
            do{
                std::string sub;
                iss >> sub;
                //printf(" token %s\n", sub.c_str());
                token.push_back(sub);
            }while(iss);
            if( (token.size() >= 10)
                && ((atoi(token[8].c_str()) & 0x3) == 3)
                && (token[9] == interface) ){
                //printf("has route is %s\n", token[9].c_str());
                // 3 means it is connected to route
                result = true;
                break;
            }
        }while(!infile.eof());
        infile.close();
    }
    return result;
}

/*   Report if the network interface is a default Route or not*/
static bool isDefault(char *nf, bool ipv6 = false)
{
    return hasRoute(nf, 0x03, ipv6);
}

// Helper function to default Route checking
static bool isValidRoute(char *nf, bool ipv6)
{
    return hasRoute(nf, -1, ipv6);
}

/* Fetch the Wireless network interface ESSID */
typedef struct
{
    std::string ssid;
    int channel;
    enum PhysicalLayerType layer;
    enum PhysicalLayerSubtype slayer;

} WIFIINFO;

static void getWifiInfo(char* ifname, bool ipv6, WIFIINFO& info)
{
    struct iw_range range;
    double freq;
    int channel;

    //0 - SSID  1-channelID 2-PhysicalLayer Subtype
    if(!isValidRoute(ifname, ipv6)){
        // Doing this to workaround some HW issue on certain Wireless adapter,
        // link is down but report its connected
        info.ssid="";
    }

    int sockfd;
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        info.ssid="";
    }

    // Get Layer type
    struct iwreq pwrq;
    memset(&pwrq, 0, sizeof(pwrq));
    strncpy(pwrq.ifr_name, ifname, IFNAMSIZ);
    if (ioctl(sockfd, SIOCGIWNAME, &pwrq) != -1) {
        std::string protocol(pwrq.u.name, IFNAMSIZ);
        //printf("protocol: %s\n", protocol.c_str());
        info.layer = PHYSLAYER_TYPE_WIFI;
    } else {
        // not a wifi interface. return out.
        info.layer = PHYSLAYER_TYPE_WIRED;
        close(sockfd);
        return;
    }

    struct iwreq wreq;
    memset(&wreq, 0, sizeof(struct iwreq));
    strncpy(wreq.ifr_name, ifname, IFNAMSIZ-1);   //Set interface name
    wreq.ifr_name[IFNAMSIZ-1] = '\0';

    char buffer[32];
    memset(buffer, 0, 32);
    wreq.u.essid.pointer = buffer;
    wreq.u.essid.length = 32;
    // Get SSID
    if (ioctl(sockfd,SIOCGIWESSID, &wreq) == -1) {
        info.ssid="";
    } else
        info.ssid= std::string((char*)wreq.u.essid.pointer);


    // Get Current channel ID
    if(nflx_wifi_get_range_info(sockfd, ifname, &range) < 0)
        info.channel=-1;
    else
    {
        if(nflx_wifi_get_ext(sockfd, ifname, SIOCGIWFREQ, &wreq) >= 0)
        {
            freq = nflx_wifi_freq2float(&(wreq.u.freq));
            channel = nflx_wifi_freq_to_channel(freq, &range);
            info.channel=channel;
        }
    }
    //Get Protocol info
    //Partner TODO: Please query the network layer to obtain the current Wifi protocol in use e.g. 802.11a,b,g or n
    info.slayer=PHYSLAYER_SUBTYPE_UNKNOWN;
    close(sockfd);
}

/*  Check if network interface has good internet connection, applicable to only devices with Connectivity manager middleware e.g. Android, iOS*/
static ConnectionState isInternetConnected(char* nf)
{
    NRDP_UNUSED(nf);
    return CONN_STATE_UNKNOWN;
}

/*  Check if  network interface has been physically enabled or not */
static ConnectionState isLinkConnected(char* nf)
{
    char uri[400]="/sys/class/net/";
    int r;
    strcat(uri,nf);
    strcat(uri,"/carrier");
    if(FILE* fp =fopen(uri,"r")) {
        char link[10];
        r = fscanf(fp,"%s",link);
        fclose(fp);
        if(r == 1 && !strcmp(link,"1"))
            return CONN_STATE_CONNECTED;
    }
    return CONN_STATE_DISCONNECTED;
}

std::vector<ISystem::NetworkInterface> FileSystem::getNetworkInterfaces()
{
    std::vector<ISystem::NetworkInterface> listnf;

    WIFIINFO info;
    struct ISystem::NetworkInterface nf;

    DIR *d;
    struct dirent *dir;
    d = opendir("/sys/class/net/");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") && strcmp(dir->d_name,"..")) {
                nf.ipAddress = getIPAddress(std::string(dir->d_name));
                nf.ipv6Addresses = getIPv6Address(dir->d_name);
                nf.macAddress = getMACAddress(dir->d_name);
                nf.name = std::string(dir->d_name);
                nf.linkConnected =isLinkConnected(dir->d_name);
                nf.internetConnected = isInternetConnected(dir->d_name);
                if (nf.ipAddress.empty() && nf.ipv6Addresses.size()) {
                    // ipv6
                    nf.isDefault = isDefault(dir->d_name, true);
                    getWifiInfo(dir->d_name, true, info);
                } else {
                    // ipv4
                    nf.isDefault = isDefault(dir->d_name, false);
                    getWifiInfo(dir->d_name, false, info);
                }

                nf.physicalLayerType = info.layer;
                if (info.layer == PHYSLAYER_TYPE_WIFI) {
                    // wifi
                    nf.ssid = info.ssid;
                    nf.wirelessChannel = info.channel;
                    nf.physicalLayerSubtype = info.slayer;
                } else {
                    // wired
                    nf.ssid = "";
                    nf.wirelessChannel = -1;
                    //Partner TODO: Please query the network layer to obtain the current Wifi protocol
                    nf.physicalLayerSubtype = PHYSLAYER_SUBTYPE_UNKNOWN;
                }
                listnf.push_back(nf);
            }
        }
        closedir(d);
    }

    return listnf;
}
