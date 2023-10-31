#include "mongo_pool.hpp"

#ifndef MONGOC_STATIC
#define MONGOC_STATIC
#endif
#ifndef BSON_STATIC
#define BSON_STATIC
#endif

#include <mongoc/mongoc.h>

#include <stdexcept>
#include <utility>
#include <vector>

namespace tmf {
namespace {

nlohmann::ordered_json simplify_extended(nlohmann::ordered_json value) {
    if (value.is_array()) {
        for (auto& element : value) {
            element = simplify_extended(std::move(element));
        }
        return value;
    }
    if (!value.is_object()) {
        return value;
    }
    if (value.size() == 1) {
        if (value.contains("$oid") && value["$oid"].is_string()) {
            return value["$oid"];
        }
        if (value.contains("$date") && value["$date"].is_string()) {
            return value["$date"];
        }
        if (value.contains("$numberInt") && value["$numberInt"].is_string()) {
            return std::stoi(value["$numberInt"].get<std::string>());
        }
        if (value.contains("$numberLong") && value["$numberLong"].is_string()) {
            return std::stoll(value["$numberLong"].get<std::string>());
        }
        if (value.contains("$numberDouble") && value["$numberDouble"].is_string()) {
            return std::stod(value["$numberDouble"].get<std::string>());
        }
    }
    for (auto it = value.begin(); it != value.end(); ++it) {
        it.value() = simplify_extended(it.value());
    }
    return value;
}

void append_json_value(bson_t* parent, const char* key, const nlohmann::ordered_json& value) {
    if (value.is_null()) {
        bson_append_null(parent, key, -1);
    } else if (value.is_boolean()) {
        bson_append_bool(parent, key, -1, value.get<bool>());
    } else if (value.is_number_integer()) {
        const auto number = value.get<int64_t>();
        if (number >= INT32_MIN && number <= INT32_MAX) {
            bson_append_int32(parent, key, -1, static_cast<int32_t>(number));
        } else {
            bson_append_int64(parent, key, -1, number);
        }
    } else if (value.is_number_unsigned()) {
        const auto number = value.get<uint64_t>();
        if (number <= static_cast<uint64_t>(INT32_MAX)) {
            bson_append_int32(parent, key, -1, static_cast<int32_t>(number));
        } else {
            bson_append_int64(parent, key, -1, static_cast<int64_t>(number));
        }
    } else if (value.is_number_float()) {
        bson_append_double(parent, key, -1, value.get<double>());
    } else if (value.is_string()) {
        const auto text = value.get<std::string>();
        bson_append_utf8(parent, key, -1, text.c_str(), static_cast<int>(text.size()));
    } else if (value.is_array()) {
        bson_t child;
        bson_append_array_begin(parent, key, -1, &child);
        int index = 0;
        for (const auto& element : value) {
            const auto index_key = std::to_string(index++);
            append_json_value(&child, index_key.c_str(), element);
        }
        bson_append_array_end(parent, &child);
    } else if (value.is_object()) {
        bson_t child;
        bson_append_document_begin(parent, key, -1, &child);
        for (auto it = value.begin(); it != value.end(); ++it) {
            append_json_value(&child, it.key().c_str(), it.value());
        }
        bson_append_document_end(parent, &child);
    }
}

bson_t* json_to_bson(const nlohmann::ordered_json& value) {
    bson_t* document = bson_new();
    if (!value.is_object()) {
        return document;
    }
    for (auto it = value.begin(); it != value.end(); ++it) {
        append_json_value(document, it.key().c_str(), it.value());
    }
    return document;
}

nlohmann::ordered_json bson_to_json(const bson_t* document) {
    size_t length = 0;
    char* text = bson_as_relaxed_extended_json(document, &length);
    if (!text) {
        throw std::runtime_error("Unable to read MongoDB document");
    }
    nlohmann::ordered_json parsed = nlohmann::ordered_json::parse(text, text + length);
    bson_free(text);
    return simplify_extended(std::move(parsed));
}

struct BsonGuard {
    bson_t* value = nullptr;
    explicit BsonGuard(bson_t* document) : value(document) {}
    ~BsonGuard() {
        if (value) {
            bson_destroy(value);
        }
    }
    BsonGuard(const BsonGuard&) = delete;
    BsonGuard& operator=(const BsonGuard&) = delete;
};

class Session {
public:
    explicit Session(_mongoc_client_pool_t* pool) : pool_(pool), client_(mongoc_client_pool_pop(pool)) {
        if (!client_) {
            throw std::runtime_error("Unable to acquire a MongoDB client");
        }
    }
    ~Session() {
        for (auto* collection : collections_) {
            mongoc_collection_destroy(collection);
        }
        if (client_) {
            mongoc_client_pool_push(pool_, client_);
        }
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    mongoc_collection_t* collection(const std::string& name) {
        auto* handle = mongoc_client_get_collection(client_, "TMF_FORUM_POC", name.c_str());
        if (!handle) {
            throw std::runtime_error("Unable to open MongoDB collection");
        }
        collections_.push_back(handle);
        return handle;
    }

private:
    _mongoc_client_pool_t* pool_ = nullptr;
    mongoc_client_t* client_ = nullptr;
    std::vector<mongoc_collection_t*> collections_;
};

}  // namespace

MongoPool::MongoPool() {
    mongoc_init();
    uri_ = mongoc_uri_new("mongodb://localhost:27017");
    if (!uri_) {
        throw std::runtime_error("Invalid MongoDB URI");
    }
    pool_ = mongoc_client_pool_new(uri_);
    if (!pool_) {
        throw std::runtime_error("Unable to create MongoDB client pool");
    }
}

MongoPool::~MongoPool() {
    if (pool_) {
        mongoc_client_pool_destroy(pool_);
    }
    if (uri_) {
        mongoc_uri_destroy(uri_);
    }
    mongoc_cleanup();
}

nlohmann::ordered_json MongoPool::find(const std::string& collection, const nlohmann::ordered_json& filter) {
    Session session(pool_);
    BsonGuard query(json_to_bson(filter));
    mongoc_cursor_t* cursor = mongoc_collection_find_with_opts(session.collection(collection), query.value, nullptr, nullptr);
    nlohmann::ordered_json rows = nlohmann::ordered_json::array();
    const bson_t* document = nullptr;
    while (mongoc_cursor_next(cursor, &document)) {
        rows.push_back(bson_to_json(document));
    }
    bson_error_t error;
    if (mongoc_cursor_error(cursor, &error)) {
        mongoc_cursor_destroy(cursor);
        throw std::runtime_error(error.message);
    }
    mongoc_cursor_destroy(cursor);
    return rows;
}

void MongoPool::insert_one(const std::string& collection, const nlohmann::ordered_json& document) {
    Session session(pool_);
    BsonGuard body(json_to_bson(document));
    bson_error_t error;
    if (!mongoc_collection_insert_one(session.collection(collection), body.value, nullptr, nullptr, &error)) {
        throw std::runtime_error(error.message);
    }
}

void MongoPool::delete_all(const std::string& collection) {
    Session session(pool_);
    BsonGuard query(bson_new());
    bson_error_t error;
    if (!mongoc_collection_delete_many(session.collection(collection), query.value, nullptr, nullptr, &error)) {
        throw std::runtime_error(error.message);
    }
}

void MongoPool::upsert(const std::string& collection, const nlohmann::ordered_json& filter, const nlohmann::ordered_json& update) {
    Session session(pool_);
    BsonGuard query(json_to_bson(filter));
    BsonGuard body(json_to_bson(update));
    BsonGuard options(BCON_NEW("upsert", BCON_BOOL(true)));
    bson_error_t error;
    if (!mongoc_collection_update_one(session.collection(collection), query.value, body.value, options.value, nullptr, &error)) {
        throw std::runtime_error(error.message);
    }
}

std::string MongoPool::new_id() const {
    bson_oid_t oid;
    bson_oid_init(&oid, nullptr);
    char text[25];
    bson_oid_to_string(&oid, text);
    return text;
}

}  // namespace tmf
