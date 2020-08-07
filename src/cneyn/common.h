#ifndef CNEYN_COMMON_H
#define CNEYN_COMMON_H

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"

typedef size_t neyn_size;

enum neyn_error
{
    neyn_error_none,
    neyn_error_general_error,
    neyn_error_socket_error,
    neyn_error_socket_create,
    neyn_error_socket_listen,
    neyn_error_socket_accept,
    neyn_error_set_reuse,
    neyn_error_set_nonblock,
    neyn_error_set_address,
    neyn_error_epoll_create,
    neyn_error_epoll_add,
    neyn_error_epoll_wait,
    neyn_error_timer_create,
    neyn_error_thread_create,
    neyn_error_wrong_parameter,
};

enum neyn_status
{
    neyn_status_continue,
    neyn_status_switching_protocols,
    neyn_status_processing,
    neyn_status_ok,
    neyn_status_created,
    neyn_status_accepted,
    neyn_status_non_authoritative_information,
    neyn_status_no_content,
    neyn_status_reset_content,
    neyn_status_partial_content,
    neyn_status_multi_status,
    neyn_status_already_reported,
    neyn_status_im_used,
    neyn_status_multiple_choices,
    neyn_status_moved_permanently,
    neyn_status_found,
    neyn_status_see_other,
    neyn_status_not_modified,
    neyn_status_use_proxy,
    neyn_status_temporary_redirect,
    neyn_status_permanent_redirect,
    neyn_status_bad_request,
    neyn_status_unauthorized,
    neyn_status_payment_required,
    neyn_status_forbidden,
    neyn_status_not_found,
    neyn_status_method_not_allowed,
    neyn_status_not_acceptable,
    neyn_status_proxy_authentication_required,
    neyn_status_request_timeout,
    neyn_status_conflict,
    neyn_status_gone,
    neyn_status_length_required,
    neyn_status_precondition_failed,
    neyn_status_payload_too_large,
    neyn_status_request_uri_too_long,
    neyn_status_unsupported_media_type,
    neyn_status_requested_range_not_satisfiable,
    neyn_status_expectation_failed,
    neyn_status_im_a_teapot,
    neyn_status_misdirected_request,
    neyn_status_unprocessable_entity,
    neyn_status_locked,
    neyn_status_failed_dependency,
    neyn_status_upgrade_required,
    neyn_status_precondition_required,
    neyn_status_too_many_requests,
    neyn_status_request_header_fields_too_large,
    neyn_status_connection_closed_without_response,
    neyn_status_unavailable_for_legal_reasons,
    neyn_status_client_closed_request,
    neyn_status_internal_server_error,
    neyn_status_not_implemented,
    neyn_status_bad_gateway,
    neyn_status_service_unavailable,
    neyn_status_gateway_timeout,
    neyn_status_http_version_not_supported,
    neyn_status_variant_also_negotiates,
    neyn_status_insufficient_storage,
    neyn_status_loop_detected,
    neyn_status_not_extended,
    neyn_status_network_authentication_required,
    neyn_status_network_connect_timeout_error,
};

extern const char *neyn_method_body[3];
extern const char *neyn_method_list[6];
extern const char *neyn_status_code[63];
extern const char *neyn_status_phrase[63];

struct neyn_header
{
    neyn_size name_len, value_len;
    char *name_ptr, *value_ptr;
};

struct neyn_request
{
    uint16_t port, major, minor;
    neyn_size method_len, path_len, body_len, header_len;
    char *method_ptr, *path_ptr, *body_ptr;
    struct neyn_header *header_ptr;
    char address[16];
};

struct neyn_output
{
    neyn_size body_len, body_idx;
    char *body_ptr;
};

struct neyn_response
{
    enum neyn_status status;
    neyn_size body_len, header_len;
    char *body_ptr;
    struct neyn_header *header_ptr;
};

void neyn_response_init(struct neyn_response *response);

void neyn_response_write(struct neyn_response *response, struct neyn_output *output);

int neyn_string_icmp(const char *str1, const char *str2, neyn_size len);

#endif  // CNEYN_COMMON_H
