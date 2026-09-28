# TMF Forum Backend

A C++ backend for mapping customer JSON data to the fields defined by a TM Forum API schema. It lets a user inspect an API operation, upload customer data, configure field mappings and defaults, and generate JSON using the selected TMF field names.

The application prepares JSON; it does not send the generated request to a live TMF API.

## What it does

- Downloads and stores a TMF API schema from a URL supplied by an admin.
- Extracts the schema’s API titles, paths, HTTP methods, parameters, and response definitions.
- Stores customer JSON data in MongoDB.
- Lets users map customer fields to TMF fields and set default values.
- Uses saved search criteria and mappings to generate a JSON response.
- Provides a separate workflow for assembling request JSON from selected user fields and required parameters.

## How it works

1. **Import a schema.** An admin submits a schema URL. The backend downloads the JSON and stores it in MongoDB.
2. **Prepare the API choices.** reads the stored schemas and saves their paths, methods, request parameters, and response definitions in separate collections.
3. **Choose an operation.** The frontend can request available titles, paths, methods, status codes, and fields. The selected operation is stored.
4. **Upload customer data.** stores JSON records under a chosen table name.
5. **Configure the mapping.** The user selects which customer fields supply TMF fields, adds defaults, and saves lookup conditions.
6. **Generate JSON. Finds matching customer records, copies mapped values into TMF-named fields, fills configured defaults, and returns the JSON object.



## Requirements

- CMake 3.20 or newer
- A C++17 compiler
- Git, used by CMake to fetch dependencies
- MongoDB running at `localhost:27017`

The application uses the `TMF_FORUM_POC` MongoDB database. Its CMake build fetches `nlohmann/json`, `cpp-httplib`, and `mongo-c-driver`.

## Build and test

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release