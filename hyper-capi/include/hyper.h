#ifndef _HYPER_H
#define _HYPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef struct hyper_clientconn hyper_clientconn;

typedef struct hyper_clientconn_options hyper_clientconn_options;

typedef struct hyper_io hyper_io;

typedef struct hyper_request hyper_request;

typedef struct hyper_response hyper_response;

typedef struct hyper_headers hyper_headers;

typedef struct hyper_body hyper_body;

typedef struct hyper_buf hyper_buf;

typedef struct hyper_task hyper_task;

typedef struct hyper_context hyper_context;

typedef struct hyper_waker hyper_waker;

typedef struct hyper_executor hyper_executor;

// A string reference.
//
// The data in here typically is not owned by this struct.
typedef struct hyper_str {
	const uint8_t *buf;
	size_t len;
} hyper_str;

typedef enum {
    HYPERE_OK,
    HYPERE_KABOOM,
} hyper_error;

typedef enum {
	HYPER_IT_CONTINUE,
	HYPER_IT_BREAK,
} hyper_iter_step;

typedef enum {
	HYPER_POLL_READY,
	HYPER_POLL_PENDING,
} hyper_poll;

// HTTP ClientConn

// Starts an HTTP client connection handshake using the provided IO transport
// and options.
//
// Both the `io` and the `options` are consumed in this function call.
//
// The returned `hyper_task *` must be polled with an executor until the
// handshake completes, at which point the value can be taken.
hyper_task *hyper_clientconn_handshake(hyper_io *io, hyper_clientconn_options *options);

// Send a request on the client connection.
//
// Returns a task that needs to be polled until it is ready. When ready, the
// task yields a `hyper_response *`.
hyper_task *hyper_clientconn_send(hyper_clientconn *client, hyper_request *request);

// Creates a new set of HTTP clientconn options to be used in a handshake.
hyper_clientconn_options *hyper_clientconn_options_new();

// Set the client background task executor.
//
// This does not consume the `options` or the `exec`.
void hyper_clientconn_options_exec(hyper_clientconn_options *options, hyper_executor *exec);

// Frees options not passed to a handshake.
void hyper_clientconn_options_free(hyper_clientconn_options *options);

// HTTP IO

// Create a new IO type used to represent a transport.
//
// The read and write functions of this transport should be set with
// `hyper_io_set_read` and `hyper_io_set_write`.
hyper_io *hyper_io_new();

// Set the user data pointer for this IO to some value.
//
// This value is passed as an argument to the read and write callbacks.
void hyper_io_set_data(hyper_io *io, void *userdata);

#define HYPER_IO_PENDING 0xFFFFFFFF
#define HYPER_IO_ERROR 0xFFFFFFFE

// Set the read function for this IO transport.
//
// Data that is read from the transport should be put in the `buf` pointer,
// up to `buf_len` bytes. The number of bytes read should be the return value.
//
// If there is no data currently available, a waker should be claimed from
// the `ctx` and registered with whatever polling mechanism is used to signal
// when data is available later on. The return value should be
// `HYPER_IO_PENDING`.
//
// If there is an irrecoverable error reading data, then `HYPER_IO_ERROR`
// should be the return value.
void hyper_io_set_read(hyper_io *io, size_t (*func)(void *userdata, hyper_context *ctx, uint8_t *buf, size_t buf_len));

// Set the write function for this IO transport.
//
// Data from the `buf` pointer should be written to the transport, up to
// `buf_len` bytes. The number of bytes written should be the return value.
//
// If no data can currently be written, the `waker` should be cloned and
// registered with whatever polling mechanism is used to signal when data
// is available later on. The return value should be `HYPER_IO_PENDING`.
//
// If there is an irrecoverable error reading data, then `HYPER_IO_ERROR`
// should be the return value.
void hyper_io_set_write(hyper_io *io, size_t (*func)(void *userdata, hyper_context *ctx, const uint8_t *buf, size_t buf_len));

// HTTP Requests

// Construct a new HTTP request.
hyper_request *hyper_request_new();

// Free an HTTP request if not going to send it on a client.
void hyper_request_free(hyper_request *request);

// Set the HTTP Method of the request.
hyper_error hyper_request_set_method(hyper_request *request, uint8_t *method, size_t method_len);

// Set the URI of the request.
hyper_error hyper_request_set_uri(hyper_request *request, uint8_t *uri, size_t uri_len);


// Gets a reference to the HTTP headers of this request
//
// This is not an owned reference, so it should not be accessed after the
// `hyper_request` has been consumed.
hyper_headers *hyper_request_headers(hyper_request *request);


// HTTP Responses

// Free an HTTP response after using it.
void hyper_response_free(hyper_response *response);

// Get the HTTP-Status code of this response.
//
// It will always be within the range of 100-599.
uint16_t hyper_response_status(hyper_response *response);

// Gets a reference to the HTTP headers of this response.
//
// This is not an owned reference, so it should not be accessed after the
// `hyper_response` has been freed.
hyper_headers *hyper_response_headers(hyper_response *response);

// Take ownership of the body of this response.
//
// It is safe to free the response even after taking ownership of its body.
hyper_body *hyper_response_body(hyper_response *response);

// HTTP Headers

// Sets the header with the provided name to the provided value.
//
// This overwrites any previous value set for the header.
void hyper_headers_set(hyper_headers *headers, hyper_str name, hyper_str value);

// Adds the provided value to the list of the provided name.
//
// If there were already existing values for the name, this will append the
// new value to the internal list.
void hyper_headers_add(hyper_headers *headers, hyper_str name, hyper_str value);

// Iterates the headers passing each name and value pair to the callback.
//
// The `userdata` pointer is also passed to the callback.
//
// The callback should return `HYPER_IT_CONTINUE` to keep iterating, or
// `HYPER_IT_BREAK` to stop.
void hyper_headers_iter(hyper_headers *headers,
                        hyper_iter_step (*func)(void *userdata,
                                                hyper_str name,
                                                hyper_str value),
                        void *userdata);

// HTTP Body

// Sets the `userdata` that is passed to the `poll` callback.
void hyper_body_set_data(hyper_body *body, void *userdata);

// Set the poll function for this body.
//
// This function will be called each time more data is desired to write to
// the transport. Use `hyper_body_set_data` to set the `userdata` argument.
void hyper_body_set_poll(hyper_body *body,
                         hyper_poll (*func)(void *userdata,
                                      hyper_context *ctx));

// Return a task that will yield the next chunk of bytes of the body, when
// available.
//
// This task has a return type tag of `HYPER_TASK_BODY_NEXT`, which will
// give a value of `hyper_buf *`.
//
// When the body is complete, the task's value will be `NULL`.
hyper_task *hyper_body_next(hyper_body *body);

// TODO: hyper_body_trailers()

// Free this buffer.
void hyper_buf_free(hyper_buf *buf);

// Get a reference to the bytes of this buf.
//
// The returned `hyper_str` is not safe to use after freeing the `hyper_buf *`.
hyper_str hyper_buf_str(hyper_buf *buf);

// Futures and Executors

// Creates a new task executor.
hyper_executor *hyper_executor_new();

// Push a task onto the executor.
hyper_error hyper_executor_push(hyper_executor *executor, hyper_task *task);

// Polls the executor, trying to make progress on any tasks that have notified
// that they are ready again.
//
// If ready, returns a task from the executor that has completed.
//
// If there are no ready tasks, this returns `NULL`.
hyper_task *hyper_executor_poll(hyper_executor *executor);

// Frees an executor, and any tasks it may currently be holding.
void hyper_executor_free(hyper_executor *executor);

typedef enum {
	HYPER_TASK_BG,
	HYPER_TASK_ERROR,
	HYPER_TASK_CLIENTCONN,
	HYPER_TASK_RESPONSE,
} hyper_task_return_type;

// Query the return type of this task.
hyper_task_return_type hyper_task_type(hyper_task *task);

// Takes the output value of this task.
//
// This must only be called once polling the task on an executor has finished
// this task.
//
// Use `hyper_task_type` to determine the type of the `void *` return value.
void *hyper_task_value(hyper_task *task);

// Free a task.
void hyper_task_free(hyper_task *task);

// Copies a waker out of the task context.
hyper_waker *hyper_context_waker(hyper_context *ctx);

// Wakes a task waker.
//
// This signals to hyper that the relevant task can do more work.
//
// This *consumes* the waker.
void hyper_waker_wake(hyper_waker *waker);

// Free a waker that hasn't been woken.
void hyper_waker_free(hyper_waker *waker);

#ifdef __cplusplus
}
#endif

#endif