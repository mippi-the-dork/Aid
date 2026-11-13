#pragma once

#include "ShowFlags.h"

// Registered once when AidEditor loads, independently of whether the Aid tab is open.
// Each editor viewport owns its own show-flag state; hiding Aid never modifies its data.
namespace AidViewportShowFlag
{
    inline TCustomShowFlag<EShowFlagShippingValue::ForceDisabled> Flag(
        TEXT("Aid"), true, SFG_Developer, NSLOCTEXT("Aid", "ShowFlag", "Aid"));
}
