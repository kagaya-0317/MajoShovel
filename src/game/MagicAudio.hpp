#pragma once

#include <string_view>

namespace majo {

enum class MagicElement {
    Fire,
    Ice,
    Thunder,
    Wind,
    Earth,
};

inline constexpr std::string_view MagicCommonCastCueId = "se.magic.cast";

enum class MagicElementCastCueTiming {
    MagicStart,
    PrimaryEffect,
};

struct MagicAudioProfile {
    std::string_view castCueId;
    std::string_view impactCueId;
    MagicElementCastCueTiming castCueTiming = MagicElementCastCueTiming::MagicStart;
};

[[nodiscard]] constexpr MagicAudioProfile magicAudioProfile(MagicElement element)
{
    switch (element) {
    case MagicElement::Fire:
        return {"se.magic.fire.cast", "se.magic.fire.impact", MagicElementCastCueTiming::MagicStart};
    case MagicElement::Ice:
        return {"se.magic.ice.cast", "se.magic.ice.impact", MagicElementCastCueTiming::MagicStart};
    case MagicElement::Thunder:
        return {"se.magic.thunder.cast", "se.magic.thunder.impact", MagicElementCastCueTiming::MagicStart};
    case MagicElement::Wind:
        return {"se.magic.wind.cast", "se.magic.wind.impact", MagicElementCastCueTiming::MagicStart};
    case MagicElement::Earth:
        return {"se.magic.earth.cast", "se.magic.earth.impact", MagicElementCastCueTiming::PrimaryEffect};
    }
    return {};
}

} // namespace majo
