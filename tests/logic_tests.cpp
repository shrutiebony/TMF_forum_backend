#include "logic.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        ++failures;
    }
}

}  // namespace

int main() {
    expect(tmf::validate_json_object(R"({"a":1})"), "object is valid");
    expect(!tmf::validate_json_object("[1,2]"), "array is invalid for javax.json object reader");
    expect(!tmf::validate_json_object("{"), "truncated json is invalid");
    expect(tmf::equals_ignore_case("Get", "get"), "ignore case");

    const auto flattened = tmf::convert_json_node_to_dto(nlohmann::ordered_json::parse(R"([{"name":"a","age":1},{"name":"b"}])"));
    expect(flattened.size() == 3, "flattened field count");
    expect(flattened[0]["fieldName"] == "name" && flattened[0]["fieldValue"] == "a", "first field");
    expect(flattened[1]["fieldValue"] == 1, "numeric field");

    const auto nested = tmf::convert_json_value(nlohmann::ordered_json::parse(R"({"name":"a"})"));
    expect(nested.is_array() && nested.empty(), "object values become an empty list");

    auto processed = tmf::process_json_node(nlohmann::ordered_json::parse(R"({"fieldName":"foo","fieldValue":"bar","keep":1})"));
    expect(processed.contains("fieldName") && processed["fieldName"] == "bar", "fieldName key is rewritten to the field value");
    expect(!processed.contains("fieldValue"), "fieldValue is removed");
    expect(processed["keep"] == 1, "other keys stay");

    std::vector<tmf::Parameter> parameters = {
        {"id", "string", true},
    };
    const auto overwritten = tmf::add_mandatory_fields(parameters, nlohmann::ordered_json::parse(R"([{"fieldName":"extra","fieldValue":"x"}])"));
    expect(overwritten.size() == 2, "shared object is inserted twice");
    expect(overwritten[0]->fieldName == "extra" && overwritten[1]->fieldName == "extra", "later mutation overwrites the required field");
    expect(overwritten[0].get() == overwritten[1].get(), "both entries are the same object");

    parameters.push_back({"name", "string", false});
    const auto defaults = tmf::add_mandatory_fields(
        parameters, nlohmann::ordered_json::parse(R"([{"fieldName":"name","fieldValue":"alice"}])"));
    expect(defaults.size() == 2, "match plus missing required field");
    bool saw_blank_id = false;
    bool saw_name = false;
    for (const auto& item : defaults) {
        if (item->fieldName == "id") {
            saw_blank_id = item->fieldValue == "";
        }
        if (item->fieldName == "name") {
            saw_name = item->fieldValue == "alice" && item->hasBeenGivenUserDefinedValue;
        }
    }
    expect(saw_blank_id, "required string defaults to empty");
    expect(saw_name, "matched value is kept");

    nlohmann::ordered_json documents = nlohmann::ordered_json::parse(R"([{"tableName":"orders","items":[{"sku":"a"}],"tags":["x"],"_id":"1"}])");
    const auto names = tmf::field_names_of(documents);
    expect(names.count("tableName") == 1, "scalar field");
    expect(names.count("items[0].sku") == 1, "nested array object");
    expect(names.count("tags") == 0, "scalar arrays are not field names");
    expect(names.count("_id") == 1, "id is included");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed" << std::endl;
        return 1;
    }
    std::cout << "logic tests passed" << std::endl;
    return 0;
}
