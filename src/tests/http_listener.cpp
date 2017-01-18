#include "catch.h"
extern "C" {
    #include "uv.h"
    #include "sds.h"
    #include "octane.h"
    #include "../octane/http_connection.h"
    #include "../octane/http_listener.h"
}

SCENARIO("http_listener receives http requests", "[http_listener]") {
    GIVEN("An empty buffer") {
        // Initialize octane types.
        http_connection* connection = new_http_connection();
        http_listener* listener = new_http_listener();
        connection->listener = listener;

        // Initialize libuv types.
        uv_handle_t* client = (uv_handle_t*)malloc(sizeof(uv_handle_t));
        uv_stream_t* stream = (uv_stream_t*)malloc(sizeof(uv_stream_t));
        client->data = connection;
        stream->data = connection;
        size_t suggested_size = 64000; // Matches default libuv provides.
        uv_buf_t* buffer = (uv_buf_t*)malloc(sizeof(uv_buf_t));

        WHEN("requested to allocate 64KB of memory") {
            uv_stream_on_alloc(client, suggested_size, buffer);

            THEN("a buffer with 64KB is allocated") {
                REQUIRE(buffer->len == 64000);

                AND_WHEN("requested to allocate another 64KB of memory") {
                    // We simulate a read of 100 bytes.
                    // The first 50 bytes represent a partial request.
                    // The second 50 bytes represent the completion of the request.
                    connection->current_buffer_position = 100;
                    connection->current_parsed_position = 50;

                    uv_buf_t* buffer2 = (uv_buf_t*)malloc(sizeof(uv_buf_t));
                    uv_stream_on_alloc(client, suggested_size, buffer2);

                    THEN("the previous buffer is reused from the previous offset") {
                        REQUIRE(buffer2->base == buffer->base + 50);
                        REQUIRE(buffer2->len == 64000 - 50);
                    }
                }
            }
        }

        WHEN("2 complete http request is received in the buffer") {
            // Request new buffer.
            uv_stream_on_alloc(client, suggested_size, buffer);

            buffer->base = "GET /plaintext HTTP/1.0\r\n" \
                    "Host: localhost:8000\r\n" \
                    "User-Agent: curl/7.49.1\r\n" \
                    "Accept: */*\r\n" \
                    "\r\n" \
                    "POST /json HTTP/1.1\r\n" \
                    "Host: localhost:8000\r\n" \
                    "User-Agent: curl/7.49.1\r\n" \
                    "Accept: */*\r\n" \
                    "\r\n";

            THEN("2 http request is parsed") {
                listener->request_cb = [](
                        http_connection* connection,
                        http_request** requests,
                        int number_of_requests) {

                    REQUIRE(number_of_requests == 2);

                    // Verify the first request.
                    REQUIRE(sdscmp(requests[0]->path, sdsnew("/plaintext")) == 0);
                    REQUIRE(sdscmp(requests[0]->method, sdsnew("GET")) == 0);
                    REQUIRE(requests[0]->version == 0);

                    // Verify the second request.
                    REQUIRE(sdscmp(requests[1]->path, sdsnew("/json")) == 0);
                    REQUIRE(sdscmp(requests[1]->method, sdsnew("POST")) == 0);
                    REQUIRE(requests[1]->version == 1);

                };
                uv_stream_on_read(stream, strlen(buffer->base), buffer);
            }
        }
    }
}