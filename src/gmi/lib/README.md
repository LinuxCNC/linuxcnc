# GMI - Generated Message Interface Runtime Library

This library provides common utilities for GMI-generated code:

- **gmi_error** - Error codes and handling
- **gmi_types** - Dynamic buffers, string slices, configuration constants
- **gmi_json** - JSON parsing/building (wraps cJSON)
- **gmi_http** - HTTP client for REST calls (wraps libcurl)

## Dependencies

Both dependencies are required and checked by `./configure`.

### libcurl

```bash
# Debian/Ubuntu
sudo apt install libcurl4-openssl-dev

# Fedora/RHEL
sudo dnf install libcurl-devel
```

### libcjson

```bash
# Debian/Ubuntu
sudo apt install libcjson-dev

# Fedora/RHEL
sudo dnf install cjson-devel
```

## Building

The library is built automatically by the main Makefile via `src/gmi/lib/Submakefile`.

Output: `lib/libgmi.so` and `lib/libgmi.so.0`

## Usage

Include the main header in your code:

```c
#include <gmi.h>
```

Link against the library:

```makefile
LDFLAGS += -lgmi -lcurl
```

## Configuration

Constants can be overridden at compile time:

| Define | Default | Description |
|--------|---------|-------------|
| `GMI_URL_MAX` | 2048 | Maximum URL length |
| `GMI_HTTP_TIMEOUT` | 30 | Default HTTP timeout (seconds) |
| `GMI_RESPONSE_INITIAL_SIZE` | 4096 | Initial response buffer size |

Example:
```bash
make EXTRAFLAGS="-DGMI_URL_MAX=4096"
```

## API Overview

### Error Handling

```c
const char *gmi_strerror(int err);  // Get error message
// Error codes: GMI_OK, GMI_ERR_ALLOC, GMI_ERR_CURL, GMI_ERR_JSON, ...
```

### Dynamic Buffers

```c
gmi_buf_t buf;
gmi_buf_init(&buf, 1024);
gmi_buf_append_str(&buf, "hello");
gmi_buf_printf(&buf, " %d", 42);
// buf.data = "hello 42", buf.size = 8
gmi_buf_free(&buf);
```

### JSON Helpers

```c
// Parsing
cJSON *json = gmi_json_parse(str);
int32_t val = gmi_json_get_i32(json, "count", 0);
const char *name = gmi_json_get_str(json, "name");

// Building
cJSON *obj = gmi_json_object();
gmi_json_add_str(obj, "name", "test");
gmi_json_add_i32(obj, "count", 42);
char *str = gmi_json_to_str(obj);
```

### HTTP Client

```c
gmi_http_t *http = gmi_http_new("http://localhost:8080/api");

// Simple GET
cJSON *data = gmi_http_get_json(http, "/pins", NULL);

// Request builder for complex requests
gmi_request_t *req = gmi_request_new(http, GMI_HTTP_GET, "/pin/{name}");
gmi_request_path_param(req, "name", "axis.0.pos-cmd");
gmi_request_execute(req);
cJSON *result = gmi_request_response_json(req);
gmi_request_free(req);

gmi_http_free(http);
```
