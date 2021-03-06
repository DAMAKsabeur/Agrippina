/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef DEVICE_SYSTEMVALUES_H
#define DEVICE_SYSTEMVALUES_H

#include <nrdbase/Variant.h>

namespace netflix {
namespace device {

class SystemValues
{
public:
    static Variant getSystemValue(const std::string& key);
};

}
}

#endif
