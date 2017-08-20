/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#include "SystemValues.h"

#include <fstream>

#ifdef TEXTTOSPEECH_REFERENCE_IVONA
#include <ivona_tts.h>
#endif
#ifdef TEXTTOSPEECH_REFERENCE_FESTIVAL
#include <festival.h>
#endif

using namespace netflix;
using namespace netflix::device;

Variant SystemValues::getSystemValue(const std::string& key)
{
    Variant map;
    map["apiVersion"] = 1;
    if (key == "all" || key == "cpu") {
        struct CpuInfo
        {
            CpuInfo() : architecture(""),
                        speedInMhz(-1),
                        coreCount(-1),
                        L1CacheSizeInKb(-1),
                        L2CacheSizeInKb(-1),
                        supportFPU(false)
            {
            }

            std::string architecture;
            int32_t speedInMhz; // in MHz
            int32_t coreCount;
            int32_t L1CacheSizeInKb; // in KB
            int32_t L2CacheSizeInKb; // in KB
            bool    supportFPU;
        };

        Variant cpu;
#if defined(NRDP_PLATFORM_LINUX)
        CpuInfo cpuInfo;
        int32_t processCount = 0;
        std::string line;
        std::size_t found;

        // proc/cpuinfo
        std::ostringstream cpuinfo_file;
        cpuinfo_file << "/proc/cpuinfo";
        std::ifstream cpuinfo(cpuinfo_file.str().c_str());
        std::string cpuId;
        if( !cpuinfo.fail() && !cpuinfo.eof()) {
            while( std::getline(cpuinfo, line) ) {
                // arch
                found = line.find("model name");
                if( found != std::string::npos ) {
                    found = line.find(":");
                    cpuInfo.architecture = line.substr(found + 1);
                    continue;
                }
                // core count
                found = line.find("cpu cores");
                if( found != std::string::npos ) {
                    found = line.find(":");
                    processCount += atoi(line.substr(found + 1).c_str());
                    continue;
                }
                // fpu
                found = line.find("fpu");
                if( found != std::string::npos ) {
                    if (line.find("yes") != std::string::npos) {
                        cpuInfo.supportFPU = true;
                        continue;
                    }
                }

                // maxSpeed
                found = line.find("processor");
                if( found != std::string::npos ) {
                    found = line.find(":");
                    cpuId = line.substr(found + 1);
                    size_t start = cpuId.find_first_not_of(" ");
                    cpuId = cpuId.substr(start);
                    std::string maxSpeedPath = "/sys/devices/system/cpu/cpu" + cpuId + "/cpufreq/scaling_max_freq";
                    std::ifstream cpuMaxSpeedIfs(maxSpeedPath.c_str());
                    std::string maxSpeed;
                    std::getline(cpuMaxSpeedIfs, maxSpeed);
                    cpuInfo.speedInMhz = ( atoi(maxSpeed.c_str()) + 500 ) / 1000;
                }
            }
            cpuInfo.coreCount = processCount;
        }
        /*
         * L1 Cache
         */
        std::ostringstream L1Cache_file;
        L1Cache_file << "/sys/devices/system/cpu/cpu0/cache/index1/size";
        std::ifstream L1Cache(L1Cache_file.str().c_str());
        if(! L1Cache.fail() && !L1Cache.eof()) {
            while( std::getline(L1Cache, line) ) {
                found = line.find("K");
                cpuInfo.L1CacheSizeInKb = atoi(line.substr(0,found).c_str());
                continue;
            }
        }
        /*
         * L2 Cache
         */
        std::ostringstream L2Cache_file;
        L2Cache_file << "/sys/devices/system/cpu/cpu0/cache/index2/size";
        std::ifstream L2Cache(L2Cache_file.str().c_str());
        if( !L2Cache.fail() && !L2Cache.eof())
        {
            while( std::getline(L2Cache, line) ) {
                found = line.find_first_of("kK");
                cpuInfo.L2CacheSizeInKb = atoi(line.substr(0,found).c_str());
                continue;
            }
        }

        /*
         * If above system files are not accessible, cpuInfo will have initial
         * values set from constructor, which should be considered as invalid by any
         * consumer of this information
         */
        cpu["architecture"] = cpuInfo.architecture;
        cpu["speedInMHz"] = cpuInfo.speedInMhz;
        cpu["coreCount"] = cpuInfo.coreCount;
        cpu["l1CacheSizeInKB"] = cpuInfo.L1CacheSizeInKb;
        cpu["l2CacheSizeinKB"] = cpuInfo.L2CacheSizeInKb;
        cpu["supportFPU"] = cpuInfo.supportFPU;
#endif
        map["cpu"] = cpu;
    }
    if (key == "all" || key == "memory") {
        Variant memory;
#if defined(NRDP_PLATFORM_LINUX)
        std::string line;
        std::size_t found;

        std::ostringstream memoryInfo_file;
        memoryInfo_file << "/proc/meminfo";
        std::ifstream meminfo(memoryInfo_file.str().c_str());
        if( !meminfo.fail() && !meminfo.eof())
        {
            while( std::getline(meminfo, line) ) {
                found = line.find("MemTotal");
                if( found != std::string::npos ) {
                    found = line.find(":");
                    line = line.substr(found + 1);
                    found = line.find_first_of("kK");
                    line = line.substr(0,found);
                    memory["physicalSystemSizeInKB"] = atoi(line.c_str());
                    break;
                }
            }
        }
#endif
        map["memory"] = memory;
    }
    if (key == "all" || key == "power") {
        Variant power;
#if defined(NRDP_PLATFORM_LINUX)
        // https://www.kernel.org/doc/Documentation/power/states.txt
        const char * powermanagement = "/sys/power/state";

        std::ifstream file(powermanagement);
        std::string str;
        std::getline(file, str);

        power["supportSuspendToIdle"] = str.find("freeze") != std::string::npos;
        power["supportStandby"] = str.find("standby") != std::string::npos;
        power["supportSuspendToRam"] = str.find("mem") != std::string::npos;
        power["supportSuspendToDisk"] = str.find("disk") != std::string::npos;
#endif
        map["power"] = power;
    }
    if (key == "all" || key == "input") {
        Variant input;
        input["supportsPointer"] = true;
        input["supportsTouch"] = false;
        input["supportsKeyboard"] = true;
        map["input"] = input;
    }
    if (key == "all" || key == "accessibility") {
        Variant accessibility;
        Variant tts;
#ifdef TEXTTOSPEECH_REFERENCE_IVONA
        tts["name"] = "Ivona";
        tts["version"] = tts_vendor_tag();
#endif
#ifdef TEXTTOSPEECH_REFERENCE_FESTIVAL
        tts["name"] = "Festival";
        tts["version"] = festival_version;
#endif
        accessibility["textToSpeechEngine"] = tts;
        map["accessibility"] = accessibility;
    }
    if (key == "all" || key == "network") {
        Variant network;
#if defined(NRDP_PLATFORM_LINUX)
        Variant interface;
        std::string deviceList = "/proc/net/dev";
        std::string networkInfoPath = "/sys/class/net/";

        std::ifstream file(deviceList.c_str());
        std::string devices;
        while (std::getline(file, devices)) {
            size_t found = devices.find(":");
            if( found != std::string::npos ) {
                size_t start = devices.find_first_not_of(" ");
                std::string s = devices.substr(start, found-start);

                Variant property;
                // get wakeon support
                {
                    std::string additionalPath = "/device/power/wakeup";
                    std::string targetPath = networkInfoPath + s + additionalPath;
                    std::ifstream file2(targetPath.c_str());
                    std::string wakeInfo;
                    std::getline(file2, wakeInfo);
                    property["wake"] = wakeInfo.find("enabled") != std::string::npos;
                }
                // get bus type
                {
                    std::string additionalPath = "/device/modalias";
                    std::string targetPath = networkInfoPath + s + additionalPath;
                    std::ifstream file2(targetPath.c_str());
                    std::string busInfo;
                    std::getline(file2, busInfo);
                    size_t found2 = busInfo.find(":");
                    property["bus"] = (found2 != std::string::npos)?busInfo.substr(0, found2):"other";
                }
                // get phsyical layer type
                {
                    // check if it is virtual
                    char symlink[1024];
                    int count;
                    count = readlink((networkInfoPath + s).c_str(), symlink, 1023);
                    // read symlink to find out more about the interface
                    if ( count <= 0 ) {
                        property["type"] = "other";
                    } else {
                        // set '\0' just for in case
                        symlink[1023] = '\0';

                        // check if it is a virtual interface
                        if ( strstr(symlink, "virtual") != NULL ) {
                            property["type"] = "virtual";
                        } else {
                            // now check if it is wireless
                            std::string additionalPath = "/wireless";
                            std::string targetPath = networkInfoPath + s + additionalPath;
                            if (access(targetPath.c_str(), F_OK ) != -1 ) {
                                property["type"] = "wifi";
                            } else {
                                property["type"] = "wired";
                            }
                        }
                    }
                }
                interface[s] = property;
                continue;
            }
        }

        const char * kernel_ipv6 = "/proc/net/if_inet6";
        if (access(kernel_ipv6, F_OK ) != -1 ) {
            network["supportIpv6"] = true;
        }

        network["interfaces"] = interface;

        const char * rmem_max = "/proc/sys/net/core/rmem_max";
        if(access(rmem_max, F_OK ) != -1){
            std::ifstream file(rmem_max);
            std::string str;
            std::getline(file, str);
            network["rmem_max"] = atoi(str.c_str());
        }

#endif
        map["network"] = network;
    }
    if (key == "all" || key == "allocator") {
        Variant allocator;
        allocator["javascriptcore"] = "system";
        allocator["streaming"] = "system";
        map["allocator"] = allocator;
    }
    return map;
}
