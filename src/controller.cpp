#include "controller.hpp"

#include "http_get.hpp"
#include "logic.hpp"

#include <iostream>
#include <set>
#include <stdexcept>

namespace tmf {
namespace {

constexpr const char* kAllJsonInputs = "AllJsonInputs";
constexpr const char* kFinalResponse = "finalResponseStructureDocument";
constexpr const char* kPathRelations = "tmfJsonStructure_Evaluated";
constexpr const char* kPathHttp = "pathHttpMethodMapping";
constexpr const char* kFinalRequest = "finalRequestStructureDocument";
constexpr const char* kUserChosen = "UserChosenTMFParameters";
constexpr const char* kAllUserInputs = "AllUserInputs";
constexpr const char* kTmfMapping = "TmfToCustomerDataMapping";
constexpr const char* kSearchCriteria = "databaseSearchCriteria";
constexpr const char* kTableMapping = "TableMapping";
constexpr const char* kJsonUri = "JsonUriDocument";
constexpr const char* kAdmin = "adminCreds";

constexpr const char* kClassAllJson = "com.TMF.Forum.Project.Organiser.Document.AllJsonInputsDocument";
constexpr const char* kClassAllUser = "com.TMF.Forum.Project.Organiser.Document.AllUserInputsDocument";
constexpr const char* kClassKv = "com.TMF.Forum.Project.Organiser.DTO.UserJsonInputKeyValuePairDTO";
constexpr const char* kClassRequest = "com.TMF.Forum.Project.Organiser.Document.FinalRequestStructureDocument";
constexpr const char* kClassParameter = "com.TMF.Forum.Project.Organiser.DTO.ParameterDTO";
constexpr const char* kClassResponse = "com.TMF.Forum.Project.Organiser.Document.FinalResponseStructureDocument";
constexpr const char* kClassResponseSchema = "com.TMF.Forum.Project.Organiser.DTO.ResponseSchemaDTO";
constexpr const char* kClassSchema = "com.TMF.Forum.Project.Organiser.DTO.SchemaDTO";
constexpr const char* kClassItems = "com.TMF.Forum.Project.Organiser.DTO.ItemsDTO";
constexpr const char* kClassPathHttp = "com.TMF.Forum.Project.Organiser.Document.PathHttpMethodMappingDocument";
constexpr const char* kClassRelations = "com.TMF.Forum.Project.Organiser.Document.PathRelationDefinerDocument_AllRelationsDocument";
constexpr const char* kClassChosen = "com.TMF.Forum.Project.Organiser.Document.TMF_RequestInputDocument";
constexpr const char* kClassJsonUri = "com.TMF.Forum.Project.Organiser.Document.JsonUriDocuments";
constexpr const char* kClassTable = "com.TMF.Forum.Project.Organiser.Document.TableMappingDocument";
constexpr const char* kClassMapping = "com.TMF.Forum.Project.Organiser.Document.TmfToCustomerDataMappingDocument";
constexpr const char* kClassCustomerMap = "com.TMF.Forum.Project.Organiser.DTO.CustomerTableDetailsMappingWithTMF";
constexpr const char* kClassDefault = "com.TMF.Forum.Project.Organiser.DTO.DefaultValuesDTO";
constexpr const char* kClassCriteria = "com.TMF.Forum.Project.Organiser.Document.DatabaseSearchCriteriaDocument";

HttpResult text_result(std::string body, int status = 200) {
    return {status, std::move(body), "text/plain;charset=UTF-8"};
}

HttpResult json_result(const nlohmann::ordered_json& body, int status = 200) {
    return {status, body.dump(), "application/json"};
}

nlohmann::ordered_json must_parse(const std::string& body) {
    return nlohmann::ordered_json::parse(body);
}

const nlohmann::ordered_json* first_or_null(const nlohmann::ordered_json& rows) {
    if (!rows.is_array() || rows.empty()) {
        return nullptr;
    }
    return &rows[0];
}

nlohmann::ordered_json text_or_null(const nlohmann::ordered_json& object, const char* key) {
    if (!object.contains(key) || object[key].is_null()) {
        return nullptr;
    }
    return object[key];
}

std::string text_or_empty(const nlohmann::ordered_json& node) {
    if (node.is_string()) {
        return node.get<std::string>();
    }
    if (node.is_number() || node.is_boolean()) {
        return node.dump();
    }
    return "";
}

nlohmann::ordered_json schema_to_api(const nlohmann::ordered_json& schema) {
    if (!schema.is_object()) {
        return nullptr;
    }
    nlohmann::ordered_json items = nullptr;
    if (schema.contains("items") && schema["items"].is_object()) {
        items = nlohmann::ordered_json::object();
        items["$ref"] = schema["items"].contains("$ref") ? schema["items"]["$ref"] : nullptr;
    }
    return {
        {"type", schema.contains("type") ? schema["type"] : nullptr},
        {"items", items},
        {"$ref", schema.contains("$ref") ? schema["$ref"] : nullptr},
    };
}

nlohmann::ordered_json parameter_to_api(const nlohmann::ordered_json& parameter) {
    nlohmann::ordered_json schema = nullptr;
    if (parameter.contains("schema") && parameter["schema"].is_object()) {
        schema = schema_to_api(parameter["schema"]);
    }
    return {
        {"name", text_or_null(parameter, "name")},
        {"description", text_or_null(parameter, "description")},
        {"required", parameter.contains("required") ? parameter["required"] : false},
        {"in", text_or_null(parameter, "in")},
        {"type", text_or_null(parameter, "type")},
        {"schema", schema},
    };
}

nlohmann::ordered_json encode_field_value(const nlohmann::ordered_json& value) {
    if (!value.is_array()) {
        return value;
    }
    nlohmann::ordered_json encoded = nlohmann::ordered_json::array();
    for (const auto& element : value) {
        if (element.is_object() && element.contains("fieldName")) {
            nlohmann::ordered_json item = {{"_class", kClassKv}, {"fieldName", element["fieldName"]}};
            if (element.contains("fieldValue") && !element["fieldValue"].is_null()) {
                item["fieldValue"] = encode_field_value(element["fieldValue"]);
            }
            encoded.push_back(std::move(item));
        } else {
            encoded.push_back(encode_field_value(element));
        }
    }
    return encoded;
}

nlohmann::ordered_json decode_field_value(const nlohmann::ordered_json& value) {
    if (value.is_array()) {
        nlohmann::ordered_json decoded = nlohmann::ordered_json::array();
        for (const auto& element : value) {
            if (element.is_object() && element.contains("fieldName")) {
                nlohmann::ordered_json item = {
                    {"fieldName", element["fieldName"]},
                    {"fieldValue", element.contains("fieldValue") ? decode_field_value(element["fieldValue"]) : nullptr},
                };
                decoded.push_back(std::move(item));
            } else {
                decoded.push_back(decode_field_value(element));
            }
        }
        return decoded;
    }
    if (value.is_object()) {
        nlohmann::ordered_json copy = value;
        copy.erase("_class");
        for (auto it = copy.begin(); it != copy.end(); ++it) {
            it.value() = decode_field_value(it.value());
        }
        return copy;
    }
    return value;
}

nlohmann::ordered_json user_data_to_api(const nlohmann::ordered_json& user_data) {
    if (!user_data.is_array()) {
        throw std::runtime_error("userData is missing");
    }
    nlohmann::ordered_json rows = nlohmann::ordered_json::array();
    for (const auto& item : user_data) {
        rows.push_back({
            {"fieldName", item.contains("fieldName") ? item["fieldName"] : nullptr},
            {"fieldValue", item.contains("fieldValue") ? decode_field_value(item["fieldValue"]) : nullptr},
        });
    }
    return rows;
}

std::vector<Parameter> parameters_from_document(const nlohmann::ordered_json& structure) {
    if (!structure.is_object() || !structure.contains("parameters") || !structure["parameters"].is_array()) {
        throw std::runtime_error("parameters are missing");
    }
    std::vector<Parameter> parameters;
    for (const auto& item : structure["parameters"]) {
        Parameter parameter;
        parameter.name = item.value("name", "");
        parameter.type = item.contains("type") ? item["type"] : nullptr;
        parameter.required = item.value("required", false);
        parameters.push_back(std::move(parameter));
    }
    return parameters;
}

nlohmann::ordered_json stored_parameter(const nlohmann::ordered_json& parameter_node) {
    nlohmann::ordered_json parameter = {{"_class", kClassParameter}};
    parameter["name"] = parameter_node.value("name", "");
    if (parameter_node.contains("description") && !parameter_node["description"].is_null()) {
        parameter["description"] = parameter_node["description"];
    }
    parameter["required"] = parameter_node.value("required", false);
    if (parameter_node.contains("type") && parameter_node["type"].is_string()) {
        parameter["type"] = parameter_node["type"];
    } else {
        parameter["type"] = "SchemaObject";
    }
    return parameter;
}

nlohmann::ordered_json stored_schema(const nlohmann::ordered_json& schema_node) {
    if (!schema_node.is_object()) {
        return nullptr;
    }
    nlohmann::ordered_json schema = {{"_class", kClassSchema}};
    if (schema_node.contains("type") && !schema_node["type"].is_null()) {
        schema["type"] = schema_node["type"];
    }
    if (schema_node.contains("$ref") && !schema_node["$ref"].is_null()) {
        schema["$ref"] = schema_node["$ref"];
    }
    if (schema_node.contains("items") && schema_node["items"].is_object()) {
        nlohmann::ordered_json items = {{"_class", kClassItems}};
        if (schema_node["items"].contains("$ref") && !schema_node["items"]["$ref"].is_null()) {
            items["$ref"] = schema_node["items"]["$ref"];
        }
        schema["items"] = std::move(items);
    }
    return schema;
}

nlohmann::ordered_json mapping_entry(const nlohmann::ordered_json& item) {
    nlohmann::ordered_json stored = {{"_class", kClassCustomerMap}};
    if (item.contains("tmfFieldName")) stored["tmfFieldName"] = item["tmfFieldName"];
    if (item.contains("customerTableName")) stored["customerTableName"] = item["customerTableName"];
    if (item.contains("customerFieldName")) stored["customerFieldName"] = item["customerFieldName"];
    return stored;
}

nlohmann::ordered_json default_entry(const nlohmann::ordered_json& item) {
    nlohmann::ordered_json stored = {{"_class", kClassDefault}};
    if (item.contains("tmfFieldName")) stored["tmfFieldName"] = item["tmfFieldName"];
    if (item.contains("defaultValue")) stored["defaultValue"] = item["defaultValue"];
    return stored;
}

bool document_has_key(const nlohmann::ordered_json& document, const std::string& key) {
    return document.is_object() && document.contains(key);
}

}  // namespace

Controller::Controller(MongoPool& database) : db_(database) {}

HttpResult Controller::give_back_array1() {
    return json_result(nlohmann::ordered_json::array({"Apple", "Pineapple", "Banana"}));
}

HttpResult Controller::give_back_array2() {
    return json_result(nlohmann::ordered_json::array({"Gamma", "Alpha", "Beta"}));
}

HttpResult Controller::persist_all() {
    try {
        db_.delete_all(kFinalResponse);
        db_.delete_all(kPathRelations);
        db_.delete_all(kPathHttp);
        db_.delete_all(kFinalRequest);
        const auto rows = db_.find(kAllJsonInputs, nlohmann::ordered_json::object());
        nlohmann::ordered_json relations = nlohmann::ordered_json::array();
        for (const auto& row : rows) {
            try {
                if (!row.contains("data") || !row["data"].is_object()) {
                    throw std::runtime_error("data is missing");
                }
                const auto& data = row["data"];
                const auto& paths = data.at("paths");
                const std::string title = data.at("info").at("title").get<std::string>();

                nlohmann::ordered_json path_methods = nlohmann::ordered_json::array();
                nlohmann::ordered_json requests = nlohmann::ordered_json::array();
                nlohmann::ordered_json responses = nlohmann::ordered_json::array();

                for (auto path_it = paths.begin(); path_it != paths.end(); ++path_it) {
                    nlohmann::ordered_json methods = nlohmann::ordered_json::array();
                    if (path_it.value().is_object()) {
                        for (auto method_it = path_it.value().begin(); method_it != path_it.value().end(); ++method_it) {
                            methods.push_back(method_it.key());
                        }
                    }
                    nlohmann::ordered_json path_doc = {
                        {"_class", kClassPathHttp},
                        {"endPoint", title},
                        {"pathName", path_it.key()},
                        {"allMethodTypes", methods},
                    };
                    db_.insert_one(kPathHttp, path_doc);
                    path_methods.push_back(path_doc);

                    if (!path_it.value().is_object()) {
                        continue;
                    }
                    for (auto method_it = path_it.value().begin(); method_it != path_it.value().end(); ++method_it) {
                        const auto& method = method_it.value();
                        nlohmann::ordered_json tags = nlohmann::ordered_json::array();
                        for (const auto& tag : method.at("tags")) {
                            tags.push_back(tag.get<std::string>());
                        }
                        nlohmann::ordered_json parameters = nlohmann::ordered_json::array();
                        for (const auto& parameter : method.at("parameters")) {
                            nlohmann::ordered_json copy = parameter;
                            if (!copy.contains("type")) {
                                copy["type"] = "SchemaObject";
                            }
                            parameters.push_back(stored_parameter(copy));
                        }
                        nlohmann::ordered_json request = {
                            {"_class", kClassRequest},
                            {"endPoint", title},
                            {"operationId", method.at("operationId").get<std::string>()},
                            {"tags", tags},
                            {"parameters", parameters},
                            {"methodType", method_it.key()},
                            {"pathName", path_it.key()},
                        };
                        db_.insert_one(kFinalRequest, request);
                        requests.push_back(request);

                        nlohmann::ordered_json response_schemas = nlohmann::ordered_json::array();
                        for (auto code_it = method.at("responses").begin(); code_it != method.at("responses").end(); ++code_it) {
                            nlohmann::ordered_json response_schema = {{"_class", kClassResponseSchema}, {"statusCode", code_it.key()}};
                            if (code_it.value().contains("description") && !code_it.value()["description"].is_null()) {
                                response_schema["description"] = code_it.value()["description"];
                            }
                            if (code_it.value().contains("schema")) {
                                auto schema = stored_schema(code_it.value()["schema"]);
                                if (!schema.is_null()) {
                                    response_schema["schema"] = std::move(schema);
                                }
                            }
                            response_schemas.push_back(std::move(response_schema));
                        }
                        nlohmann::ordered_json response = {
                            {"_class", kClassResponse},
                            {"responseSchemaDTOSList", response_schemas},
                            {"methodType", method_it.key()},
                            {"pathName", path_it.key()},
                            {"endPoint", title},
                        };
                        db_.insert_one(kFinalResponse, response);
                        responses.push_back(response);
                    }
                }

                db_.insert_one(kPathRelations, {
                    {"_class", kClassRelations},
                    {"endPoint", title},
                    {"pathMethodTypeMapping", path_methods},
                    {"allJsonStructures", requests},
                    {"allResponseStructure", responses},
                });
            } catch (const std::exception& error) {
                std::cerr << error.what() << std::endl;
            }
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        std::cout << "Not refreshed" << std::endl;
    }
    std::cout << "Refreshed" << std::endl;
    return text_result("All json structures refreshed");
}

HttpResult Controller::get_all_path_names(const std::string& end_point) {
    nlohmann::ordered_json names = nlohmann::ordered_json::array();
    for (const auto& row : db_.find(kPathHttp, {{"endPoint", end_point}})) {
        names.push_back(row.value("pathName", ""));
    }
    return json_result(names);
}

HttpResult Controller::get_all_method_types(const std::string& end_point, const std::string& path_name) {
    const auto rows = db_.find(kPathHttp, {{"endPoint", end_point}, {"pathName", path_name}});
    const auto* row = first_or_null(rows);
    if (!row || !row->contains("allMethodTypes")) {
        throw std::runtime_error("method types are missing");
    }
    return json_result((*row)["allMethodTypes"]);
}

HttpResult Controller::get_all_response_statuses(const std::string& end_point, const std::string& path_name, const std::string& method_type) {
    const auto rows = db_.find(kFinalResponse, {{"endPoint", end_point}, {"pathName", path_name}, {"methodType", method_type}});
    const auto* row = first_or_null(rows);
    if (!row || !row->contains("responseSchemaDTOSList")) {
        throw std::runtime_error("response statuses are missing");
    }
    nlohmann::ordered_json codes = nlohmann::ordered_json::array();
    for (const auto& schema : (*row)["responseSchemaDTOSList"]) {
        codes.push_back(schema.value("statusCode", ""));
    }
    return json_result(codes);
}

nlohmann::ordered_json Controller::request_structure(const std::string& end_point, const std::string& path_name, const std::string& method_type, bool null_end_point) {
    nlohmann::ordered_json filter = {{"pathName", path_name}, {"methodType", method_type}};
    filter["endPoint"] = null_end_point ? nlohmann::ordered_json(nullptr) : nlohmann::ordered_json(end_point);
    const auto rows = db_.find(kFinalRequest, filter);
    const auto* row = first_or_null(rows);
    if (!row) {
        throw std::runtime_error("request structure is missing");
    }
    return *row;
}

nlohmann::ordered_json Controller::parameters_of(const nlohmann::ordered_json& structure) {
    if (!structure.contains("parameters") || !structure["parameters"].is_array()) {
        throw std::runtime_error("parameters are missing");
    }
    nlohmann::ordered_json parameters = nlohmann::ordered_json::array();
    for (const auto& parameter : structure["parameters"]) {
        parameters.push_back(parameter_to_api(parameter));
    }
    return parameters;
}

HttpResult Controller::show_tmf_structure(const std::string& endpoint, const std::string& path_name, const std::string& method_type) {
    return json_result(parameters_of(request_structure(endpoint, path_name, method_type)));
}

HttpResult Controller::store_chosen_parameters(const std::string& body) {
    const auto input = must_parse(body);
    db_.delete_all(kUserChosen);
    nlohmann::ordered_json document = {{"_class", kClassChosen}};
    if (input.contains("endPoint") && !input["endPoint"].is_null()) document["endPoint"] = input["endPoint"];
    if (input.contains("pathName") && !input["pathName"].is_null()) document["pathName"] = input["pathName"];
    if (input.contains("methodType") && !input["methodType"].is_null()) document["methodType"] = input["methodType"];
    if (input.contains("responseType") && !input["responseType"].is_null()) document["responseType"] = input["responseType"];
    db_.insert_one(kUserChosen, document);
    return json_result({
        {"endPoint", input.contains("endPoint") ? input["endPoint"] : nullptr},
        {"pathName", input.contains("pathName") ? input["pathName"] : nullptr},
        {"methodType", input.contains("methodType") ? input["methodType"] : nullptr},
        {"responseType", input.contains("responseType") ? input["responseType"] : nullptr},
    });
}

HttpResult Controller::store_user_input(const std::string& body) {
    if (!validate_json_object(body)) {
        return text_result("Invalid Json");
    }
    try {
        const auto root = nlohmann::ordered_json::parse(body);
        nlohmann::ordered_json document = {{"_class", kClassAllUser}, {"_id", db_.new_id()}};
        document["tableName"] = root.at("table_Name").dump();
        const auto inner = nlohmann::ordered_json::parse(root.at("textareaContent").get<std::string>());
        document["userData"] = encode_field_value(convert_json_node_to_dto(inner));
        db_.insert_one(kAllUserInputs, document);
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
    }
    return text_result("File uploaded");
}

HttpResult Controller::delete_all_user_input() {
    db_.delete_all(kAllUserInputs);
    db_.delete_all(kTmfMapping);
    db_.delete_all(kSearchCriteria);
    db_.delete_all(kTableMapping);
    return text_result("All inputs deleted");
}

HttpResult Controller::add_mandatory_fields(const std::string& body) {
    const auto input = must_parse(body);
    const bool null_end_point = !input.contains("endPoint") || input["endPoint"].is_null();
    const auto structure = request_structure(
        null_end_point || !input["endPoint"].is_string() ? "" : input["endPoint"].get<std::string>(),
        input.contains("pathName") && input["pathName"].is_string() ? input["pathName"].get<std::string>() : "",
        input.contains("methodType") && input["methodType"].is_string() ? input["methodType"].get<std::string>() : "",
        null_end_point);
    const auto settings = tmf::add_mandatory_fields(parameters_from_document(structure), input.at("fieldNames"));
    nlohmann::ordered_json rows = nlohmann::ordered_json::array();
    for (const auto& setting : settings) {
        rows.push_back(setting_to_json(*setting));
    }
    return json_result(rows);
}

nlohmann::ordered_json Controller::chosen_document() {
    const auto rows = db_.find(kUserChosen, nlohmann::ordered_json::object());
    if (rows.empty()) {
        throw std::runtime_error("chosen parameters are missing");
    }
    return rows[0];
}

nlohmann::ordered_json Controller::chosen_dto() {
    const auto document = chosen_document();
    return {
        {"endPoint", text_or_null(document, "endPoint")},
        {"pathName", text_or_null(document, "pathName")},
        {"methodType", text_or_null(document, "methodType")},
        {"responseType", nullptr},
    };
}

HttpResult Controller::create_final_user_json(const std::string& body) {
    const auto chosen_fields = must_parse(body);
    const auto chosen = chosen_dto();
    nlohmann::ordered_json dynamic_input = {
        {"fieldNames", chosen_fields},
        {"methodType", chosen["methodType"]},
        {"pathName", chosen["pathName"]},
    };
    const auto users = db_.find(kAllUserInputs, nlohmann::ordered_json::object());
    if (users.empty()) {
        throw std::runtime_error("user input is missing");
    }
    const auto structure = request_structure(
        "",
        dynamic_input["pathName"].is_string() ? dynamic_input["pathName"].get<std::string>() : "",
        dynamic_input["methodType"].is_string() ? dynamic_input["methodType"].get<std::string>() : "",
        true);
    const auto settings = tmf::add_mandatory_fields(parameters_from_document(structure), dynamic_input["fieldNames"]);
    auto user_data = user_data_to_api(users[0].at("userData"));
    for (const auto& setting : settings) {
        user_data.push_back({{"fieldName", setting->fieldName}, {"fieldValue", setting->fieldValue}});
    }
    nlohmann::ordered_json result = nlohmann::ordered_json::object();
    for (const auto& dto : user_data) {
        result[dto["fieldName"].get<std::string>()] = dto["fieldValue"];
    }
    return json_result(process_json_node(std::move(result)));
}

HttpResult Controller::get_all_user_input() {
    const auto rows = db_.find(kAllUserInputs, nlohmann::ordered_json::object());
    if (rows.empty()) {
        throw std::runtime_error("user input is missing");
    }
    return json_result(user_data_to_api(rows[0].at("userData")));
}

HttpResult Controller::get_chosen_user_input_parameters() {
    return json_result(chosen_dto());
}

HttpResult Controller::show_tmf_structure_after_choosing() {
    const auto chosen = chosen_dto();
    return json_result(parameters_of(request_structure(
        chosen["endPoint"].is_null() ? "" : chosen["endPoint"].get<std::string>(),
        chosen["pathName"].is_null() ? "" : chosen["pathName"].get<std::string>(),
        chosen["methodType"].is_null() ? "" : chosen["methodType"].get<std::string>())));
}

HttpResult Controller::get_all_tmf_fields() {
    const auto chosen = chosen_dto();
    nlohmann::ordered_json names = nlohmann::ordered_json::array();
    try {
        const auto structure = request_structure(
            chosen["endPoint"].is_null() ? "" : chosen["endPoint"].get<std::string>(),
            chosen["pathName"].is_null() ? "" : chosen["pathName"].get<std::string>(),
            chosen["methodType"].is_null() ? "" : chosen["methodType"].get<std::string>());
        for (const auto& parameter : structure["parameters"]) {
            names.push_back(parameter.value("name", ""));
        }
    } catch (const std::runtime_error&) {
        return json_result(names);
    }
    return json_result(names);
}

HttpResult Controller::get_all_tmf_fields_mandatory() {
    const auto chosen = chosen_dto();
    nlohmann::ordered_json names = nlohmann::ordered_json::array();
    const auto rows = db_.find(kFinalRequest, {
        {"endPoint", chosen["endPoint"].is_null() ? nullptr : chosen["endPoint"]},
        {"pathName", chosen["pathName"].is_null() ? nullptr : chosen["pathName"]},
        {"methodType", chosen["methodType"].is_null() ? nullptr : chosen["methodType"]},
    });
    if (!rows.empty() && rows[0].contains("parameters")) {
        for (const auto& parameter : rows[0]["parameters"]) {
            if (parameter.value("required", false)) {
                names.push_back(parameter.value("name", ""));
            }
        }
    }
    return json_result(names);
}

HttpResult Controller::post_tmf_json_by_admin(const std::string& body) {
    try {
        for (const auto& existing : db_.find(kJsonUri, nlohmann::ordered_json::object())) {
            if (existing.contains("jsonUri") && existing["jsonUri"].is_string() &&
                equals_ignore_case(body, existing["jsonUri"].get<std::string>())) {
                return text_result("This TMF Forum schema already exists");
            }
        }
        db_.insert_one(kJsonUri, {{"_class", kClassJsonUri}, {"jsonUri", body}});
        const auto request_node = nlohmann::ordered_json::parse(body);
        const std::string url = text_or_empty(request_node.value("schema", nlohmann::ordered_json()));
        HttpGetResult response;
        try {
            response = http_get(url);
        } catch (const UriError&) {
            throw;
        } catch (const std::exception&) {
            return text_result("Something went wrong. Please try again later");
        }
        if (response.status != 200) {
            return text_result("Request failed with status code: " + std::to_string(response.status));
        }
        const auto swagger = nlohmann::ordered_json::parse(response.body);
        db_.insert_one(kAllJsonInputs, {{"_class", kClassAllJson}, {"_id", db_.new_id()}, {"data", swagger}});
    } catch (const UriError&) {
        throw;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return text_result("Something went wrong. Please try again later");
    }
    std::cout << "Saved" << std::endl;
    return text_result("Input saved successfully");
}

HttpResult Controller::get_response_json(const std::string& end_point, const std::string& path_name, const std::string& method_type, const std::string& status_code) {
    const auto rows = db_.find(kFinalResponse, {{"endPoint", end_point}, {"pathName", path_name}, {"methodType", method_type}});
    const auto* row = first_or_null(rows);
    if (!row) {
        throw std::runtime_error("response structure is missing");
    }
    nlohmann::ordered_json schema = nullptr;
    bool matched = false;
    for (const auto& item : row->value("responseSchemaDTOSList", nlohmann::ordered_json::array())) {
        if (item.contains("statusCode") && item["statusCode"].is_string() &&
            equals_ignore_case(status_code, item["statusCode"].get<std::string>())) {
            matched = true;
            schema = item.contains("schema") ? schema_to_api(item["schema"]) : nullptr;
        }
    }
    if (matched && schema.is_null()) {
        return {200, "", "application/json"};
    }
    if (!matched) {
        schema = {{"type", nullptr}, {"items", nullptr}, {"$ref", nullptr}};
    }
    return json_result(schema);
}

HttpResult Controller::get_admin_credentials() {
    const auto rows = db_.find(kAdmin, nlohmann::ordered_json::object());
    if (rows.empty()) {
        throw std::runtime_error("admin credentials are missing");
    }
    return json_result({
        {"userName", text_or_null(rows[0], "userName")},
        {"password", text_or_null(rows[0], "password")},
    });
}

HttpResult Controller::store_json_to_mongo(const std::string& body) {
    try {
        const auto input = nlohmann::ordered_json::parse(body);
        const auto textarea = input.at("textareaContent");
        if (!textarea.is_string()) {
            throw std::runtime_error("textareaContent is missing");
        }
        const auto input_json = nlohmann::ordered_json::parse(textarea.get<std::string>());
        const std::string table_name = input.contains("table_Name") ? text_or_empty(input["table_Name"]) : "";
        auto save_object = [&](nlohmann::ordered_json object) {
            object["tableName"] = table_name;
            object["_class"] = "java.util.LinkedHashMap";
            db_.insert_one(kAllUserInputs, object);
        };
        if (input_json.is_array()) {
            for (const auto& object : input_json) {
                if (!object.is_object()) {
                    throw std::runtime_error("array element is not an object");
                }
                save_object(object);
            }
        } else if (input_json.is_object()) {
            for (const auto& existing : db_.find(kAllUserInputs, nlohmann::ordered_json::object())) {
                if (existing.contains("tableName") && existing["tableName"].is_string() &&
                    existing["tableName"].get<std::string>() == table_name) {
                    return text_result("This table name already exists. Please give a different table name");
                }
            }
            save_object(input_json);
        } else {
            throw std::runtime_error("input is not an object");
        }
    } catch (const std::exception&) {
        return text_result("Something went wrong. Please contact the developer");
    }
    return text_result("Data saved successfully");
}

HttpResult Controller::get_table_names() {
    std::set<std::string> names;
    for (const auto& row : db_.find(kAllUserInputs, nlohmann::ordered_json::object())) {
        if (row.contains("tableName") && row["tableName"].is_string()) {
            names.insert(row["tableName"].get<std::string>());
        }
    }
    return json_result(names);
}

HttpResult Controller::get_table_name_corresponding_to_first(const std::string& table_name) {
    std::set<std::string> names;
    for (const auto& row : db_.find(kAllUserInputs, nlohmann::ordered_json::object())) {
        if (!row.contains("tableName") || !row["tableName"].is_string()) {
            throw std::runtime_error("tableName is missing");
        }
        const auto& name = row["tableName"].get<std::string>();
        if (!equals_ignore_case(name, table_name)) {
            names.insert(name);
        }
    }
    return json_result(names);
}

HttpResult Controller::get_field_names(const std::string& table_name) {
    const auto names = field_names_of(db_.find(kAllUserInputs, {{"tableName", table_name}}));
    return json_result(names);
}

HttpResult Controller::get_all_field_names() {
    return json_result(field_names_of(db_.find(kAllUserInputs, nlohmann::ordered_json::object())));
}

HttpResult Controller::save_table_mapping(const std::string& body) {
    try {
        const auto rows = nlohmann::ordered_json::parse(body);
        if (!rows.is_array()) {
            throw std::runtime_error("expected an array");
        }
        for (const auto& row : rows) {
            nlohmann::ordered_json document = {{"_class", kClassTable}};
            if (row.contains("id") && row["id"].is_string() && !row["id"].get<std::string>().empty()) {
                document["_id"] = row["id"];
            } else {
                document["_id"] = db_.new_id();
            }
            if (row.contains("rowNumber")) document["rowNumber"] = row["rowNumber"];
            if (row.contains("columnNumber")) document["columnNumber"] = row["columnNumber"];
            if (row.contains("fieldValue") && !row["fieldValue"].is_null()) document["fieldValue"] = row["fieldValue"];
            db_.insert_one(kTableMapping, document);
        }
        return text_result("Data saved successfully.");
    } catch (const std::exception& error) {
        return text_result("Error saving data: " + std::string(error.what()));
    }
}

HttpResult Controller::save_tmf_to_customer_mapping(const std::string& body) {
    nlohmann::ordered_json missing = nlohmann::ordered_json::array();
    try {
        const auto input = nlohmann::ordered_json::parse(body);
        std::set<std::string> chosen_names;
        nlohmann::ordered_json mapping = nlohmann::ordered_json::array();
        for (const auto& item : input.at("mapping")) {
            mapping.push_back(mapping_entry(item));
            if (item.contains("tmfFieldName") && item["tmfFieldName"].is_string()) {
                chosen_names.insert(item["tmfFieldName"].get<std::string>());
            }
        }
        const auto chosen = chosen_dto();
        const auto rows = db_.find(kFinalRequest, {
            {"endPoint", chosen["endPoint"]},
            {"pathName", chosen["pathName"]},
            {"methodType", chosen["methodType"]},
        });
        if (!rows.empty() && rows[0].contains("parameters") && rows[0]["parameters"].is_array()) {
            for (const auto& parameter : rows[0]["parameters"]) {
                const std::string name = parameter.value("name", "");
                if (parameter.value("required", false) && !chosen_names.count(name)) {
                    missing.push_back(name);
                }
            }
        }
        const nlohmann::ordered_json filter = {
            {"endPoint", input.value("endPoint", nullptr)},
            {"pathName", input.value("pathName", nullptr)},
            {"methodType", input.value("methodType", nullptr)},
        };
        db_.upsert(kTmfMapping, filter, {
            {"$set", {{"mapping", mapping}}},
            {"$setOnInsert", {{"_class", kClassMapping}}},
        });
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
    }
    return json_result(missing);
}

HttpResult Controller::save_defaults(const std::string& body) {
    try {
        const auto input = nlohmann::ordered_json::parse(body);
        nlohmann::ordered_json defaults = nlohmann::ordered_json::array();
        if (input.contains("defaults") && input["defaults"].is_array()) {
            for (const auto& item : input["defaults"]) {
                defaults.push_back(default_entry(item));
            }
        }
        db_.upsert(kTmfMapping, {
            {"endPoint", input.value("endPoint", nullptr)},
            {"pathName", input.value("pathName", nullptr)},
            {"methodType", input.value("methodType", nullptr)},
        }, {
            {"$set", {{"defaults", defaults}}},
            {"$setOnInsert", {{"_class", kClassMapping}}},
        });
        return text_result("Defaults saved successfully");
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return text_result("Unable to save the defaults due to some error.");
    }
}

HttpResult Controller::save_database_search_criteria(const std::string& body) {
    try {
        const auto rows = nlohmann::ordered_json::parse(body);
        if (!rows.is_array()) {
            throw std::runtime_error("expected an array");
        }
        for (const auto& row : rows) {
            nlohmann::ordered_json document = {{"_class", kClassCriteria}, {"_id", db_.new_id()}};
            if (row.contains("tableName")) document["tableName"] = row["tableName"];
            if (row.contains("fieldName")) document["fieldName"] = row["fieldName"];
            if (row.contains("searchCriteria")) document["searchCriteria"] = row["searchCriteria"].is_string()
                ? row["searchCriteria"]
                : nlohmann::ordered_json(row["searchCriteria"].dump());
            db_.insert_one(kSearchCriteria, document);
        }
        return text_result("Data saved successfully");
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return text_result("Error saving data: " + std::string(error.what()));
    }
}

HttpResult Controller::delete_all_search_criteria() {
    try {
        db_.delete_all(kSearchCriteria);
        return text_result("Data deleted successfully");
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return text_result("Error deleting data: " + std::string(error.what()));
    }
}

HttpResult Controller::get_final_response() {
    const auto chosen = chosen_dto();
    std::vector<std::string> output_fields;
    const auto structure = request_structure(
        chosen["endPoint"].is_null() ? "" : chosen["endPoint"].get<std::string>(),
        chosen["pathName"].is_null() ? "" : chosen["pathName"].get<std::string>(),
        chosen["methodType"].is_null() ? "" : chosen["methodType"].get<std::string>());
    for (const auto& parameter : structure["parameters"]) {
        output_fields.push_back(parameter.value("name", ""));
    }

    const auto mappings = db_.find(kTmfMapping, {
        {"endPoint", chosen["endPoint"]},
        {"pathName", chosen["pathName"]},
        {"methodType", chosen["methodType"]},
    });
    if (mappings.empty() || !mappings[0].contains("mapping") || !mappings[0]["mapping"].is_array()) {
        throw std::runtime_error("mapping is missing");
    }
    const auto& mapping = mappings[0]["mapping"];
    const auto criteria_rows = db_.find(kSearchCriteria, nlohmann::ordered_json::object());
    nlohmann::ordered_json response = nlohmann::ordered_json::object();
    std::set<std::string> mapped;

    for (const auto& criteria : criteria_rows) {
        const auto documents = db_.find(kAllUserInputs, {
            {"tableName", criteria.value("tableName", "")},
            {criteria.value("fieldName", ""), criteria.value("searchCriteria", "")},
        });
        for (const auto& tmf_field : output_fields) {
            for (const auto& entry : mapping) {
                if (!entry.contains("tmfFieldName") || !entry["tmfFieldName"].is_string() ||
                    !equals_ignore_case(entry["tmfFieldName"].get<std::string>(), tmf_field)) {
                    continue;
                }
                const std::string customer_field = entry.value("customerFieldName", "");
                if (!documents.empty() && document_has_key(documents[0], customer_field)) {
                    response[tmf_field] = documents[0][customer_field];
                    if (!documents[0][customer_field].is_null()) {
                        mapped.insert(tmf_field);
                    }
                }
            }
        }
    }

    std::vector<std::string> remaining;
    for (const auto& field : output_fields) {
        if (!mapped.count(field)) {
            remaining.push_back(field);
        }
    }
    if (mappings[0].contains("defaults") && mappings[0]["defaults"].is_array()) {
        for (const auto& field : remaining) {
            for (const auto& item : mappings[0]["defaults"]) {
                if (item.contains("tmfFieldName") && item["tmfFieldName"].is_string() &&
                    equals_ignore_case(item["tmfFieldName"].get<std::string>(), field)) {
                    response[field] = item.contains("defaultValue") ? item["defaultValue"] : nullptr;
                }
            }
        }
    }
    return json_result(response);
}

HttpResult Controller::get_all_titles() {
    std::set<std::string> titles;
    for (const auto& row : db_.find(kPathRelations, nlohmann::ordered_json::object())) {
        if (row.contains("endPoint") && row["endPoint"].is_string()) {
            titles.insert(row["endPoint"].get<std::string>());
        }
    }
    return json_result(titles);
}

}  // namespace tmf
