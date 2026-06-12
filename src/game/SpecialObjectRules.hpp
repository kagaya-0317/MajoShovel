#pragma once

#include <string_view>

namespace majo {

inline constexpr std::string_view ApprenticeWitchStaffObjectId = "item_staff_apprentice_witch";

inline bool isApprenticeWitchStaffObjectId(std::string_view objectId)
{
    return objectId == ApprenticeWitchStaffObjectId;
}

inline bool objectExcludedFromDungeonDrops(std::string_view objectId)
{
    return isApprenticeWitchStaffObjectId(objectId);
}

inline bool objectAllowedAsSpecialMerchantStock(std::string_view objectId)
{
    return isApprenticeWitchStaffObjectId(objectId);
}

}
