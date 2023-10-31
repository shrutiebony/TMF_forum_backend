#include "controller.hpp"

#include <chrono>
#include <ctime>
#include <exception>
#include <httplib.h>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace {

std::string timestamp_utc() {
    const auto now = std::chrono::system_clock::now();
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#if defined(_WIN32)
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    std::ostringstream out;
    out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << millis.count()
        << "+00:00";
    return out.str();
}

std::string spring_error(int status, const char* error, const std::string& path) {
    nlohmann::ordered_json body = {{"timestamp", timestamp_utc()}, {"status", status}, {"error", error}, {"path", path}};
    return body.dump();
}

void write_result(httplib::Response& res, const tmf::HttpResult& result) {
    res.status = result.status;
    res.set_header("Access-Control-Allow-Origin", "*");
    if (!result.body.empty() || result.status != 200) {
        res.set_content(result.body, result.content_type.c_str());
    }
}

void write_error(const httplib::Request& req, httplib::Response& res, int status, const char* error) {
    res.status = status;
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_content(spring_error(status, error, req.path), "application/json");
}

bool require_params(const httplib::Request& req, httplib::Response& res, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (!req.has_param(name)) {
            write_error(req, res, 400, "Bad Request");
            return false;
        }
    }
    return true;
}

template <typename Fn>
void invoke(const httplib::Request& req, httplib::Response& res, Fn&& fn) {
    try {
        write_result(res, fn());
    } catch (const nlohmann::ordered_json::exception& error) {
        std::cerr << error.what() << std::endl;
        write_error(req, res, 400, "Bad Request");
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        write_error(req, res, 500, "Internal Server Error");
    }
}

}  // namespace

int main() {
    tmf::MongoPool database;
    tmf::Controller controller(database);
    httplib::Server server;
    server.set_payload_max_length(64 * 1024 * 1024);

    server.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET,HEAD,POST,PUT,PATCH,DELETE,OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "*");
        res.set_header("Access-Control-Max-Age", "1800");
        if (req.method == "OPTIONS") {
            res.status = 200;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    server.Get("/giveBackArray1", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.give_back_array1(); });
    });
    server.Get("/giveBackArray2", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.give_back_array2(); });
    });
    server.Get("/persistAllTMF_FormatsBegins", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.persist_all(); });
    });
    server.Get("/getAllPathNames", [&](const httplib::Request& req, httplib::Response& res) {
        if (!require_params(req, res, {"endPoint"})) return;
        invoke(req, res, [&] { return controller.get_all_path_names(req.get_param_value("endPoint")); });
    });
    server.Get("/getAllMethodTypes", [&](const httplib::Request& req, httplib::Response& res) {
        if (!require_params(req, res, {"endPoint", "pathName"})) return;
        invoke(req, res, [&] {
            return controller.get_all_method_types(req.get_param_value("endPoint"), req.get_param_value("pathName"));
        });
    });
    server.Get("/getAllResponseStatuses", [&](const httplib::Request& req, httplib::Response& res) {
        if (!require_params(req, res, {"endPoint", "pathName", "methodType"})) return;
        invoke(req, res, [&] {
            return controller.get_all_response_statuses(req.get_param_value("endPoint"), req.get_param_value("pathName"),
                                                        req.get_param_value("methodType"));
        });
    });
    server.Get("/showTMF_StructureForSelectedUserInput", [&](const httplib::Request& req, httplib::Response& res) {
        if (!require_params(req, res, {"endpoint", "pathName", "methodType"})) return;
        invoke(req, res, [&] {
            return controller.show_tmf_structure(req.get_param_value("endpoint"), req.get_param_value("pathName"),
                                                 req.get_param_value("methodType"));
        });
    });
    server.Post("/storeTheUserChosenParameters", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.store_chosen_parameters(req.body); });
    });
    server.Post("/storeUserInput", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.store_user_input(req.body); });
    });
    server.Get("/deleteAllUserInput", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.delete_all_user_input(); });
    });
    server.Get("/AddMandatoryFields", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.add_mandatory_fields(req.body); });
    });
    server.Post("/createFinalUserJsonResponse", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.create_final_user_json(req.body); });
    });
    server.Get("/getAllUserInput", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.get_all_user_input(); });
    });
    server.Get("/getChosenUserInputParameters", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.get_chosen_user_input_parameters(); });
    });
    server.Get("/showTMF_Structure_AfterChoosingParameters", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.show_tmf_structure_after_choosing(); });
    });
    server.Get("/getAllTMF_Forum_FieldsToChooseFrom", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.get_all_tmf_fields(); });
    });
    server.Get("/getAllTMF_Forum_FieldsToChooseFromMandatory", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.get_all_tmf_fields_mandatory(); });
    });
    server.Post("/postTMFJsonByAdmin", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.post_tmf_json_by_admin(req.body); });
    });
    server.Get("/getResponseJson", [&](const httplib::Request& req, httplib::Response& res) {
        if (!require_params(req, res, {"endPoint", "pathName", "methodType", "statusCode"})) return;
        invoke(req, res, [&] {
            return controller.get_response_json(req.get_param_value("endPoint"), req.get_param_value("pathName"),
                                                req.get_param_value("methodType"), req.get_param_value("statusCode"));
        });
    });
    server.Get("/getAdminCredentials", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.get_admin_credentials(); });
    });
    server.Post("/storeJsonToMongo", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.store_json_to_mongo(req.body); });
    });
    server.Get("/get-Table-name", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.get_table_names(); });
    });
    server.Get("/getTableNameCorrespondingToFirst", [&](const httplib::Request& req, httplib::Response& res) {
        if (!require_params(req, res, {"tableName"})) return;
        invoke(req, res, [&] { return controller.get_table_name_corresponding_to_first(req.get_param_value("tableName")); });
    });
    server.Get("/get-field-names", [&](const httplib::Request& req, httplib::Response& res) {
        if (!require_params(req, res, {"tableName"})) return;
        invoke(req, res, [&] { return controller.get_field_names(req.get_param_value("tableName")); });
    });
    server.Get("/getAllFieldNames", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.get_all_field_names(); });
    });
    server.Post("/saveTableToTableMapping", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.save_table_mapping(req.body); });
    });
    server.Post("/saveTmfToCustomerMapping", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.save_tmf_to_customer_mapping(req.body); });
    });
    server.Post("/saveDefaults", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.save_defaults(req.body); });
    });
    server.Post("/saveDatabaseSearchCriteria", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.save_database_search_criteria(req.body); });
    });
    server.Get("/deleteAllSearchCriteria", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.delete_all_search_criteria(); });
    });
    server.Get("/getFinalResponse", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.get_final_response(); });
    });
    server.Get("/getAllTitles", [&](const httplib::Request& req, httplib::Response& res) {
        invoke(req, res, [&] { return controller.get_all_titles(); });
    });
    server.Get("/actuator/prometheus", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_content(
            "# HELP tmf_forum_up Whether the TMF forum service is running\n"
            "# TYPE tmf_forum_up gauge\n"
            "tmf_forum_up 1\n",
            "text/plain; version=0.0.4;charset=utf-8");
    });

    std::cout << "TMF forum backend listening on port 1001" << std::endl;
    if (!server.listen("0.0.0.0", 1001)) {
        std::cerr << "Failed to listen on port 1001" << std::endl;
        return 1;
    }
    return 0;
}
