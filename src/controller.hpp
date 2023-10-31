#pragma once

#include "mongo_pool.hpp"

#include <nlohmann/json.hpp>
#include <string>

namespace tmf {

struct HttpResult {
    int status = 200;
    std::string body;
    std::string content_type = "application/json";
};

class Controller {
public:
    explicit Controller(MongoPool& database);

    HttpResult give_back_array1();
    HttpResult give_back_array2();
    HttpResult persist_all();
    HttpResult get_all_path_names(const std::string& end_point);
    HttpResult get_all_method_types(const std::string& end_point, const std::string& path_name);
    HttpResult get_all_response_statuses(const std::string& end_point, const std::string& path_name, const std::string& method_type);
    HttpResult show_tmf_structure(const std::string& endpoint, const std::string& path_name, const std::string& method_type);
    HttpResult store_chosen_parameters(const std::string& body);
    HttpResult store_user_input(const std::string& body);
    HttpResult delete_all_user_input();
    HttpResult add_mandatory_fields(const std::string& body);
    HttpResult create_final_user_json(const std::string& body);
    HttpResult get_all_user_input();
    HttpResult get_chosen_user_input_parameters();
    HttpResult show_tmf_structure_after_choosing();
    HttpResult get_all_tmf_fields();
    HttpResult get_all_tmf_fields_mandatory();
    HttpResult post_tmf_json_by_admin(const std::string& body);
    HttpResult get_response_json(const std::string& end_point, const std::string& path_name, const std::string& method_type, const std::string& status_code);
    HttpResult get_admin_credentials();
    HttpResult store_json_to_mongo(const std::string& body);
    HttpResult get_table_names();
    HttpResult get_table_name_corresponding_to_first(const std::string& table_name);
    HttpResult get_field_names(const std::string& table_name);
    HttpResult get_all_field_names();
    HttpResult save_table_mapping(const std::string& body);
    HttpResult save_tmf_to_customer_mapping(const std::string& body);
    HttpResult save_defaults(const std::string& body);
    HttpResult save_database_search_criteria(const std::string& body);
    HttpResult delete_all_search_criteria();
    HttpResult get_final_response();
    HttpResult get_all_titles();

private:
    MongoPool& db_;

    nlohmann::ordered_json chosen_document();
    nlohmann::ordered_json chosen_dto();
    nlohmann::ordered_json request_structure(const std::string& end_point, const std::string& path_name, const std::string& method_type, bool null_end_point = false);
    nlohmann::ordered_json parameters_of(const nlohmann::ordered_json& structure);
};

}  // namespace tmf
