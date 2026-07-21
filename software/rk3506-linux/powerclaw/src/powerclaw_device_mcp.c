#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MCP_VERSION "0.1.0"
#define INPUT_MAX 8192
#define HTTP_MAX 16384
#define OUTPUT_MAX 65536
#define JSON_DEPTH_MAX 16
#define DEVICE_CORE_PORT 18080

enum json_type
{
    JSON_STRING,
    JSON_OBJECT,
    JSON_ARRAY,
    JSON_PRIMITIVE
};

struct json_span
{
    const char *start;
    const char *end;
    enum json_type type;
};

struct output
{
    char data[OUTPUT_MAX];
    size_t used;
    int failed;
};

struct tool_definition
{
    const char *name;
    const char *description;
    const char *path;
};

static const struct tool_definition tools[] = {
    {"system_status", "Read RK3506 system and HMI status", "/api/v1/status"},
    {"mqtt_status", "Read redacted MQTT configuration and owner status", "/api/v1/mqtt/status"},
    {"amp_status", "Passively read AMP and RPMsg device presence", "/api/v1/amp/status"},
    {"capabilities", "Read the device-core capability and denial policy", "/api/v1/capabilities"},
};

static void output_add(struct output *output, const char *format, ...)
{
    va_list arguments;
    int written;

    if (output->failed)
        return;
    va_start(arguments, format);
    written = vsnprintf(output->data + output->used,
                        sizeof(output->data) - output->used,
                        format, arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= sizeof(output->data) - output->used)
    {
        output->failed = 1;
        return;
    }
    output->used += (size_t)written;
}

static void output_json_string_n(struct output *output, const char *value, size_t length)
{
    output_add(output, "\"");
    for (size_t index = 0; index < length && !output->failed; index++)
    {
        unsigned char byte = (unsigned char)value[index];
        switch (byte)
        {
        case '\\': output_add(output, "\\\\"); break;
        case '"': output_add(output, "\\\""); break;
        case '\b': output_add(output, "\\b"); break;
        case '\f': output_add(output, "\\f"); break;
        case '\n': output_add(output, "\\n"); break;
        case '\r': output_add(output, "\\r"); break;
        case '\t': output_add(output, "\\t"); break;
        default:
            if (byte < 0x20)
                output_add(output, "\\u%04x", (unsigned int)byte);
            else
                output_add(output, "%c", byte);
        }
    }
    output_add(output, "\"");
}

static void output_json_string(struct output *output, const char *value)
{
    output_json_string_n(output, value, strlen(value));
}

static const char *skip_space(const char *cursor, const char *end)
{
    while (cursor < end && isspace((unsigned char)*cursor))
        cursor++;
    return cursor;
}

static int parse_string_span(const char **cursor, const char *end, struct json_span *span)
{
    const char *start = *cursor;

    if (start >= end || *start != '"')
        return -1;
    start++;
    for (const char *current = start; current < end; current++)
    {
        if ((unsigned char)*current < 0x20)
            return -1;
        if (*current == '\\')
        {
            current++;
            if (current >= end)
                return -1;
            if (*current == 'u')
            {
                for (int digit = 0; digit < 4; digit++)
                {
                    current++;
                    if (current >= end || !isxdigit((unsigned char)*current))
                        return -1;
                }
            }
            continue;
        }
        if (*current == '"')
        {
            span->start = start;
            span->end = current;
            span->type = JSON_STRING;
            *cursor = current + 1;
            return 0;
        }
    }
    return -1;
}

static int skip_json_value(const char **cursor, const char *end,
                           struct json_span *span, unsigned int depth)
{
    const char *start;
    char opener;
    char closer;

    if (depth > JSON_DEPTH_MAX)
        return -1;
    *cursor = skip_space(*cursor, end);
    if (*cursor >= end)
        return -1;
    start = *cursor;
    if (**cursor == '"')
        return parse_string_span(cursor, end, span);

    if (**cursor == '{' || **cursor == '[')
    {
        opener = **cursor;
        closer = opener == '{' ? '}' : ']';
        (*cursor)++;
        *cursor = skip_space(*cursor, end);
        if (*cursor < end && **cursor == closer)
        {
            (*cursor)++;
            span->start = start;
            span->end = *cursor;
            span->type = opener == '{' ? JSON_OBJECT : JSON_ARRAY;
            return 0;
        }
        for (;;)
        {
            struct json_span child;
            if (opener == '{')
            {
                if (parse_string_span(cursor, end, &child) != 0)
                    return -1;
                *cursor = skip_space(*cursor, end);
                if (*cursor >= end || **cursor != ':')
                    return -1;
                (*cursor)++;
            }
            if (skip_json_value(cursor, end, &child, depth + 1) != 0)
                return -1;
            *cursor = skip_space(*cursor, end);
            if (*cursor < end && **cursor == ',')
            {
                (*cursor)++;
                *cursor = skip_space(*cursor, end);
                continue;
            }
            if (*cursor < end && **cursor == closer)
            {
                (*cursor)++;
                span->start = start;
                span->end = *cursor;
                span->type = opener == '{' ? JSON_OBJECT : JSON_ARRAY;
                return 0;
            }
            return -1;
        }
    }

    while (*cursor < end && !isspace((unsigned char)**cursor) &&
           **cursor != ',' && **cursor != '}' && **cursor != ']')
        (*cursor)++;
    if (*cursor == start)
        return -1;
    span->start = start;
    span->end = *cursor;
    span->type = JSON_PRIMITIVE;
    return 0;
}

static int string_span_equals(const struct json_span *span, const char *value)
{
    size_t length = strlen(value);
    return span->type == JSON_STRING &&
           (size_t)(span->end - span->start) == length &&
           memcmp(span->start, value, length) == 0;
}

static int object_find(const struct json_span *object, const char *key,
                       struct json_span *value)
{
    const char *cursor;
    const char *end;

    if (object->type != JSON_OBJECT)
        return -1;
    cursor = object->start + 1;
    end = object->end - 1;
    cursor = skip_space(cursor, end);
    while (cursor < end)
    {
        struct json_span parsed_key;
        struct json_span parsed_value;

        if (parse_string_span(&cursor, end, &parsed_key) != 0)
            return -1;
        cursor = skip_space(cursor, end);
        if (cursor >= end || *cursor != ':')
            return -1;
        cursor++;
        if (skip_json_value(&cursor, end, &parsed_value, 1) != 0)
            return -1;
        if (string_span_equals(&parsed_key, key))
        {
            *value = parsed_value;
            return 0;
        }
        cursor = skip_space(cursor, end);
        if (cursor < end && *cursor == ',')
        {
            cursor++;
            cursor = skip_space(cursor, end);
            continue;
        }
        if (cursor != end)
            return -1;
    }
    return 1;
}

static int parse_root_object(const char *input, size_t length, struct json_span *root)
{
    const char *cursor = input;
    const char *end = input + length;

    cursor = skip_space(cursor, end);
    if (cursor >= end || *cursor != '{')
        return -1;
    if (skip_json_value(&cursor, end, root, 0) != 0 || root->type != JSON_OBJECT)
        return -1;
    cursor = skip_space(cursor, end);
    return cursor == end ? 0 : -1;
}

static int copy_plain_string(const struct json_span *span, char *output, size_t output_size)
{
    size_t length;

    if (span->type != JSON_STRING)
        return -1;
    length = (size_t)(span->end - span->start);
    if (length + 1 > output_size)
        return -1;
    for (size_t index = 0; index < length; index++)
    {
        unsigned char byte = (unsigned char)span->start[index];
        if (byte < 0x20 || byte > 0x7e || byte == '\\' || byte == '"')
            return -1;
        output[index] = (char)byte;
    }
    output[length] = '\0';
    return 0;
}

static int object_is_empty(const struct json_span *object)
{
    const char *cursor;
    const char *end;

    if (object->type != JSON_OBJECT)
        return 0;
    cursor = skip_space(object->start + 1, object->end - 1);
    end = object->end - 1;
    return cursor == end;
}

static void output_json_span(struct output *output, const struct json_span *span)
{
    if (span->type == JSON_STRING)
        output_json_string_n(output, span->start, (size_t)(span->end - span->start));
    else
        output_add(output, "%.*s", (int)(span->end - span->start), span->start);
}

static void emit_response(const struct json_span *id, const char *result_json)
{
    struct output output = {{0}, 0, 0};

    output_add(&output, "{\"jsonrpc\":\"2.0\",\"id\":");
    output_json_span(&output, id);
    output_add(&output, ",\"result\":%s}\n", result_json);
    if (!output.failed)
    {
        fwrite(output.data, 1, output.used, stdout);
        fflush(stdout);
    }
}

static void emit_error(const struct json_span *id, int code, const char *message)
{
    struct output output = {{0}, 0, 0};

    output_add(&output, "{\"jsonrpc\":\"2.0\",\"id\":");
    if (id != NULL)
        output_json_span(&output, id);
    else
        output_add(&output, "null");
    output_add(&output, ",\"error\":{\"code\":%d,\"message\":", code);
    output_json_string(&output, message);
    output_add(&output, "}}\n");
    if (!output.failed)
    {
        fwrite(output.data, 1, output.used, stdout);
        fflush(stdout);
    }
}

static int device_core_get(const char *path, char *body, size_t body_size,
                           char *error, size_t error_size)
{
    int socket_fd;
    struct sockaddr_in address;
    struct timeval timeout = {2, 0};
    char request[256];
    char response[HTTP_MAX + 1];
    size_t used = 0;
    int request_length;
    char *body_start;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        snprintf(error, error_size, "device-core socket failed");
        return -1;
    }
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(DEVICE_CORE_PORT);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(socket_fd, (struct sockaddr *)&address, sizeof(address)) != 0)
    {
        close(socket_fd);
        snprintf(error, error_size, "device-core unavailable");
        return -1;
    }
    request_length = snprintf(request, sizeof(request),
                              "GET %s HTTP/1.1\r\nHost: 127.0.0.1\r\n"
                              "Accept: application/json\r\nConnection: close\r\n\r\n",
                              path);
    if (request_length <= 0 || (size_t)request_length >= sizeof(request) ||
        send(socket_fd, request, (size_t)request_length, 0) != request_length)
    {
        close(socket_fd);
        snprintf(error, error_size, "device-core request failed");
        return -1;
    }
    while (used < HTTP_MAX)
    {
        ssize_t received = recv(socket_fd, response + used, HTTP_MAX - used, 0);
        if (received == 0)
            break;
        if (received < 0)
        {
            close(socket_fd);
            snprintf(error, error_size, "device-core response failed");
            return -1;
        }
        used += (size_t)received;
    }
    close(socket_fd);
    if (used == HTTP_MAX)
    {
        snprintf(error, error_size, "device-core response too large");
        return -1;
    }
    response[used] = '\0';
    if (strncmp(response, "HTTP/1.1 200 ", 13) != 0 &&
        strncmp(response, "HTTP/1.0 200 ", 13) != 0)
    {
        snprintf(error, error_size, "device-core rejected request");
        return -1;
    }
    body_start = strstr(response, "\r\n\r\n");
    if (body_start == NULL)
    {
        snprintf(error, error_size, "invalid device-core response");
        return -1;
    }
    body_start += 4;
    if (strlen(body_start) + 1 > body_size)
    {
        snprintf(error, error_size, "device-core body too large");
        return -1;
    }
    memcpy(body, body_start, strlen(body_start) + 1);
    return 0;
}

static const struct tool_definition *find_tool(const char *name)
{
    for (size_t index = 0; index < sizeof(tools) / sizeof(tools[0]); index++)
        if (strcmp(name, tools[index].name) == 0)
            return &tools[index];
    return NULL;
}

static void emit_tool_list(const struct json_span *id)
{
    struct output result = {{0}, 0, 0};

    output_add(&result, "{\"tools\":[");
    for (size_t index = 0; index < sizeof(tools) / sizeof(tools[0]); index++)
    {
        if (index > 0)
            output_add(&result, ",");
        output_add(&result, "{\"name\":");
        output_json_string(&result, tools[index].name);
        output_add(&result, ",\"description\":");
        output_json_string(&result, tools[index].description);
        output_add(&result,
                   ",\"inputSchema\":{\"type\":\"object\","
                   "\"properties\":{},\"additionalProperties\":false}}");
    }
    output_add(&result, "]}");
    if (result.failed)
        emit_error(id, -32603, "internal response overflow");
    else
        emit_response(id, result.data);
}

static void emit_tool_result(const struct json_span *id, const char *text, int is_error)
{
    struct output result = {{0}, 0, 0};

    output_add(&result, "{\"content\":[{\"type\":\"text\",\"text\":");
    output_json_string(&result, text);
    output_add(&result, "}],\"isError\":%s}", is_error ? "true" : "false");
    if (result.failed)
        emit_error(id, -32603, "internal response overflow");
    else
        emit_response(id, result.data);
}

static void handle_tool_call(const struct json_span *root, const struct json_span *id)
{
    struct json_span params;
    struct json_span name_span;
    struct json_span arguments;
    char name[64];
    char body[HTTP_MAX];
    char error[128];
    const struct tool_definition *tool;

    if (object_find(root, "params", &params) != 0 || params.type != JSON_OBJECT ||
        object_find(&params, "name", &name_span) != 0 ||
        copy_plain_string(&name_span, name, sizeof(name)) != 0)
    {
        emit_error(id, -32602, "invalid tools/call parameters");
        return;
    }
    if (object_find(&params, "arguments", &arguments) == 0 && !object_is_empty(&arguments))
    {
        emit_tool_result(id, "PowerClaw read tools accept no arguments", 1);
        return;
    }
    tool = find_tool(name);
    if (tool == NULL)
    {
        emit_tool_result(id, "Tool is not exposed by the PowerClaw device boundary", 1);
        return;
    }
    if (device_core_get(tool->path, body, sizeof(body), error, sizeof(error)) != 0)
    {
        emit_tool_result(id, error, 1);
        return;
    }
    emit_tool_result(id, body, 0);
}

static void handle_message(const char *line, size_t length)
{
    struct json_span root;
    struct json_span method_span;
    struct json_span id;
    char method[64];
    int id_result;

    if (parse_root_object(line, length, &root) != 0 ||
        object_find(&root, "method", &method_span) != 0 ||
        copy_plain_string(&method_span, method, sizeof(method)) != 0)
    {
        emit_error(NULL, -32700, "invalid JSON-RPC request");
        return;
    }
    id_result = object_find(&root, "id", &id);
    if (id_result != 0)
    {
        if (strcmp(method, "notifications/initialized") != 0 &&
            strcmp(method, "notifications/cancelled") != 0)
            fprintf(stderr, "powerclaw-device-mcp: ignored notification %s\n", method);
        return;
    }
    if (id.type == JSON_OBJECT || id.type == JSON_ARRAY)
    {
        emit_error(NULL, -32600, "invalid JSON-RPC id");
        return;
    }

    if (strcmp(method, "initialize") == 0)
    {
        emit_response(&id,
                      "{\"protocolVersion\":\"2024-11-05\","
                      "\"capabilities\":{\"tools\":{}},"
                      "\"serverInfo\":{\"name\":\"powerclaw-device\","
                      "\"version\":\"" MCP_VERSION "\"}}");
    }
    else if (strcmp(method, "ping") == 0)
        emit_response(&id, "{}");
    else if (strcmp(method, "tools/list") == 0)
        emit_tool_list(&id);
    else if (strcmp(method, "tools/call") == 0)
        handle_tool_call(&root, &id);
    else
        emit_error(&id, -32601, "method not found");
}

int main(void)
{
    char line[INPUT_MAX + 2];

    setvbuf(stdout, NULL, _IOLBF, 0);
    while (fgets(line, sizeof(line), stdin) != NULL)
    {
        size_t length = strlen(line);
        if (length == 0)
            continue;
        if (line[length - 1] != '\n' && !feof(stdin))
        {
            int byte;
            while ((byte = fgetc(stdin)) != '\n' && byte != EOF)
                ;
            emit_error(NULL, -32700, "request exceeds 8192 bytes");
            continue;
        }
        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';
        if (length > 0)
            handle_message(line, length);
    }
    return ferror(stdin) ? 1 : 0;
}
