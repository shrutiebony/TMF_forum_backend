#pragma once

#include <nlohmann/json.hpp>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace tmf {

nlohmann::ordered_json parse_strict(const std::string& text);
bool validate_json_object(const std::string& text);
bool equals_ignore_case(std::string_view a, std::string_view b);

nlohmann::ordered_json convert_json_node_to_dto(const nlohmann::ordered_json& node);
nlohmann::ordered_json convert_json_value(const nlohmann::ordered_json& value);
nlohmann::ordered_json process_json_node(nlohmann::ordered_json node);

struct Parameter {
    std::string name;
    nlohmann::ordered_json type = nullptr;
    bool required = false;
};

struct Setting {
    std::string fieldName;
    nlohmann::ordered_json fieldValue = nullptr;
    bool hasBeenGivenUserDefinedValue = false;
    nlohmann::ordered_json dataType = nullptr;
    bool isRequired = false;
    bool isTmfForumDefined = false;
    std::string includedInJson = "Not included";
};

nlohmann::ordered_json setting_to_json(const Setting& setting);

// Ports settingDefaultOrCustomValues, including the original shared-object updates.
std::vector<std::shared_ptr<Setting>> add_mandatory_fields(
    const std::vector<Parameter>& parameters,
    const nlohmann::ordered_json& field_names);

void extract_field_names(const nlohmann::ordered_json& document, const std::string& prefix, std::set<std::string>& field_names);
std::set<std::string> field_names_of(const nlohmann::ordered_json& documents);

}  // namespace tmf
