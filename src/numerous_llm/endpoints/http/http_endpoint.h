/* Copyright 2023 Tencent Inc.  All rights reserved.

==============================================================================*/
#pragma once

#include "numerous_llm/endpoints/base/base_endpoint.h"

namespace numerous_llm {

// The HTTP endpoint, used to receive request from http client.
class HttpEndpoint : public RpcEndpoint {
 public:
  HttpEndpoint(const EndpointConfig &endpoint_config,
               std::function<Status(int64_t, std::vector<int> &)> fetch_func,
               Channel<std::pair<Status, Request>> &request_queue);

  virtual ~HttpEndpoint() override {}

  // Listen at specific socket.
  virtual Status Start() override;

  // Close the listening socket.
  virtual Status Stop() override;

 private:
  // Wait until a request arrived.
  Status Accept(Request &req);

  // Send rsp to client.
  Status Send(const Status infer_status, const Response &rsp, httplib::Response &http_rsp);

  // Handle the http request.
  Status HandleRequest(const httplib::Request &http_req, httplib::Response &http_rsp);

 private:
  // The terminate flag to control processing loop.
  std::atomic<bool> terminated_{false};

  // The http server instance.
  httplib::Server http_server_;

  // The http server thread.
  std::thread http_server_thread_;
};

}  // namespace numerous_llm
