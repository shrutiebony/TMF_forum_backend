#include "logic.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace tmf {
namespace {

std::string lower_copy(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (unsigned char ch : value) {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
    return out;
}

}  // namespace

nlohmann::ordered_json parse_strict(const std::string& text) {
    return nlohmann::ordered_json::parse(text);
}

bool validate_json_object(const std::string& text) {
    try {
        return parse_strict(text).is_object();
    } catch (const std::exception&) {
        return false;
    }
}

bool equals_ignore_case(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

nlohmann::ordered_json convert_json_value(const nlohmann::ordered_json& value) {
    if (value.is_object() || value.is_array()) {
        return convert_json_node_to_dto(value);
    }
    if (value.is_string() || value.is_number() || value.is_boolean()) {
        return value;
    }
    return nullptr;
}

nlohmann::ordered_json convert_json_node_to_dto(const nlohmann::ordered_json& node) {
    nlohmann::ordered_json flattened = nlohmann::ordered_json::array();
    if (!node.is_array()) {
        return flattened;
    }
    for (const auto& element : node) {
        if (!element.is_object()) {
            continue;
        }
        for (auto it = element.begin(); it != element.end(); ++it) {
            nlohmann::ordered_json dto = nlohmann::ordered_json::object();
            dto["fieldName"] = it.key();
            dto["fieldValue"] = convert_json_value(it.value());
            flattened.push_back(std::move(dto));
        }
    }
    return flattened;
}

nlohmann::ordered_json process_json_node(nlohmann::ordered_json node) {
    if (node.is_object()) {
        std::vector<std::string> fields_to_modify;
        for (auto it = node.begin(); it != node.end(); ++it) {
            if (it.value().is_object() || it.value().is_array()) {
                it.value() = process_json_node(it.value());
            }
            if (it.key() == "fieldName") {
                fields_to_modify.push_back(it.key());
            }
        }
        for (const auto& field_name : fields_to_modify) {
            if (node.contains("fieldValue") && node["fieldValue"].is_string()) {
                const std::string field_value = node["fieldValue"].get<std::string>();
                node.erase("fieldName");
                node.erase("fieldValue");
                node[field_name] = field_value;
            }
        }
    } else if (node.is_array()) {
        for (auto& element : node) {
            element = process_json_node(element);
        }
    }
    return node;
}

nlohmann::ordered_json setting_to_json(const Setting& setting) {
    return {
        {"fieldName", setting.fieldName},
        {"fieldValue", setting.fieldValue},
        {"hasBeenGivenUserDefinedValue", setting.hasBeenGivenUserDefinedValue},
        {"dataType", setting.dataType},
        {"required", setting.isRequired},
        {"TMF_ForumDefined", setting.isTmfForumDefined},
        {"includedInJson", setting.includedInJson},
    };
}

std::vector<std::shared_ptr<Setting>> add_mandatory_fields(
    const std::vector<Parameter>& parameters,
    const nlohmann::ordered_json& field_names) {
    std::vector<std::string> tracking;
    std::vector<std::shared_ptr<Setting>> result;
    bool present_in_tmf = false;
    std::shared_ptr<Setting> current;

    if (!field_names.is_array()) {
        throw std::runtime_error("fieldNames is missing");
    }

    for (const auto& user_field : field_names) {
        if (!user_field.is_object() || !user_field.contains("fieldName") || !user_field["fieldName"].is_string()) {
            throw std::runtime_error("fieldName is missing");
        }
        const std::string user_name = user_field["fieldName"].get<std::string>();
        const nlohmann::ordered_json user_value = user_field.contains("fieldValue") ? user_field["fieldValue"] : nullptr;

        for (const auto& parameter : parameters) {
            current = std::make_shared<Setting>();
            if (equals_ignore_case(user_name, parameter.name)) {
                current->fieldName = parameter.name;
                current->dataType = parameter.type;
                if (!user_value.is_null()) {
                    current->fieldValue = user_value;
                    current->hasBeenGivenUserDefinedValue = true;
                }
                if (parameter.required) {
                    current->isRequired = true;
                }
                current->isTmfForumDefined = true;
                current->includedInJson = "Included";
                present_in_tmf = true;
                result.push_back(current);
                tracking.push_back(current->fieldName);
            } else if (parameter.required &&
                       std::find(tracking.begin(), tracking.end(), parameter.name) == tracking.end()) {
                current->fieldName = parameter.name;
                current->fieldValue = nullptr;
                current->isTmfForumDefined = true;
                current->isRequired = true;
                current->hasBeenGivenUserDefinedValue = false;
                current->includedInJson = "Included";
                current->dataType = parameter.type;
                result.push_back(current);
                tracking.push_back(current->fieldName);
            }
        }

        if (!present_in_tmf) {
            if (!current) {
                throw std::runtime_error("null setting");
            }
            current->fieldName = user_name;
            current->fieldValue = user_value;
            current->isTmfForumDefined = false;
            if (!current->fieldValue.is_null()) {
                current->hasBeenGivenUserDefinedValue = true;
            }
            current->includedInJson = "Included";
            result.push_back(current);
            tracking.push_back(current->fieldName);
        }
        present_in_tmf = false;
    }

    for (auto& item : result) {
        const bool type_is_string = item->dataType.is_string();
        const std::string type = type_is_string ? lower_copy(item->dataType.get<std::string>()) : std::string();
        if (item->fieldValue.is_null() && item->isRequired) {
            if (type == "integer" || type == "short" || type == "long") {
                item->fieldValue = 0;
            } else if (type == "double" || type == "float") {
                item->fieldValue = 0.0;
            } else if (type == "string") {
                item->fieldValue = "";
            } else if (type == "boolean") {
                item->fieldValue = false;
            } else {
                item->fieldValue = nullptr;
            }
        }
    }
    return result;
}

void extract_field_names(const nlohmann::ordered_json& document, const std::string& prefix, std::set<std::string>& field_names) {
    if (!document.is_object()) {
        return;
    }
    for (auto it = document.begin(); it != document.end(); ++it) {
        const std::string field_name = prefix + it.key();
        const auto& value = it.value();
        if (value.is_object()) {
            extract_field_names(value, field_name + ".", field_names);
        } else if (value.is_array()) {
            for (size_t i = 0; i < value.size(); ++i) {
                if (value[i].is_object()) {
                    extract_field_names(value[i], field_name + "[" + std::to_string(i) + "].", field_names);
                }
            }
        } else {
            field_names.insert(field_name);
        }
    }
}

std::set<std::string> field_names_of(const nlohmann::ordered_json& documents) {
    std::set<std::string> names;
    if (!documents.is_array()) {
        return names;
    }
    for (const auto& document : documents) {
        extract_field_names(document, "", names);
    }
    return names;
}

}  // namespace tmf
