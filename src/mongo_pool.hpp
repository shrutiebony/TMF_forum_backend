#pragma once

#include <nlohmann/json.hpp>
#include <string>

struct _mongoc_client_pool_t;
struct _mongoc_uri_t;

namespace tmf {

class MongoPool {
public:
    MongoPool();
    ~MongoPool();
    MongoPool(const MongoPool&) = delete;
    MongoPool& operator=(const MongoPool&) = delete;

    nlohmann::ordered_json find(const std::string& collection, const nlohmann::ordered_json& filter);
    void insert_one(const std::string& collection, const nlohmann::ordered_json& document);
    void delete_all(const std::string& collection);
    void upsert(const std::string& collection, const nlohmann::ordered_json& filter, const nlohmann::ordered_json& update);
    std::string new_id() const;

private:
    _mongoc_uri_t* uri_ = nullptr;
    _mongoc_client_pool_t* pool_ = nullptr;
};

}  // namespace tmf
