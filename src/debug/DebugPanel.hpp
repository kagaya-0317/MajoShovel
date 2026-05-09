#pragma once

#include <string>
#include <vector>

namespace majo {

enum class DebugControlKind {
    Button,
    Toggle,
    NumberInput,
    TextInput,
    Dropdown,
    Slider,
    Gauge,
    Label,
};

struct DebugControlDefinition {
    DebugControlKind kind = DebugControlKind::Button;
    std::string id;
    std::string label;
    std::string command;
    int minValue = 0;
    int maxValue = 100;
    std::vector<std::string> options;
};

struct DebugGroupDefinition {
    std::string id;
    std::string label;
    std::vector<DebugControlDefinition> controls;
};

struct DebugTabDefinition {
    std::string id;
    std::string label;
    std::vector<DebugGroupDefinition> groups;
};

struct DebugConsoleLayout {
    std::vector<DebugTabDefinition> tabs;
};

DebugConsoleLayout makeDefaultDebugConsoleLayout();

}
