#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <boost/config.hpp>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <sstream>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

std::atomic<size_t> request_count{0};

// Simple JSON creation function
std::string make_json(const std::string& status, size_t count) {
    std::ostringstream ss;
    ss << "{\"status\":\"" << status << "\",\"requests_handled\":" << count << "}";
    return ss.str();
}

std::string make_json_response(const std::string& message) {
    std::ostringstream ss;
    ss << "{\"status\":\"success\",\"message\":\"" << message 
       << "\",\"processed_length\":" << message.length() << "}";
    return ss.str();
}

// This function produces a JSON response for the / endpoint
std::string handle_get_request() {
    return make_json("success", request_count.fetch_add(1, std::memory_order_seq_cst));
}

// Simple JSON parsing - finds value for a key in a JSON string
std::string extract_message(const std::string& json_str) {
    size_t pos = json_str.find("\"message\"");
    if (pos == std::string::npos) return "";
    
    pos = json_str.find(':', pos);
    if (pos == std::string::npos) return "";
    
    pos = json_str.find('\"', pos);
    if (pos == std::string::npos) return "";
    
    size_t end = json_str.find('\"', pos + 1);
    if (end == std::string::npos) return "";
    
    return json_str.substr(pos + 1, end - pos - 1);
}

// This function handles the POST /data endpoint
std::string handle_post_request(const std::string& body) {
    std::string message = extract_message(body);
    request_count.fetch_add(1, std::memory_order_seq_cst);
    return make_json_response(message);
}

// HTTP session class to handle individual connections
class http_session : public std::enable_shared_from_this<http_session> {
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;
    std::shared_ptr<void> res_;

public:
    explicit http_session(tcp::socket&& socket)
        : stream_(std::move(socket)) {}

    void start() { read_request(); }

private:
    void read_request() {
        auto self = shared_from_this();

        http::async_read(
            stream_, buffer_, req_,
            [self](beast::error_code ec, std::size_t) {
                if (!ec) self->process_request();
            });
    }

    void process_request() {
        auto response = std::make_shared<http::response<http::string_body>>();
        response->version(req_.version());
        response->keep_alive(false);
        response->set(http::field::server, "Boost Beast Server");
        response->set(http::field::content_type, "application/json");

        std::string response_body;
        if (req_.method() == http::verb::get && req_.target() == "/") {
            response_body = handle_get_request();
            response->result(http::status::ok);
        }
        else if (req_.method() == http::verb::post && req_.target() == "/data") {
            response_body = handle_post_request(req_.body());
            response->result(http::status::ok);
        }
        else {
            response->result(http::status::not_found);
            response_body = "{\"error\": \"Not Found\"}";
        }

        response->body() = response_body;
        response->prepare_payload();

        http::async_write(
            stream_, *response,
            [self = shared_from_this(), response](beast::error_code ec, std::size_t) {
                self->stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
            });
    }
};

// Accepts incoming connections and launches sessions
class listener : public std::enable_shared_from_this<listener> {
    net::io_context& ioc_;
    tcp::acceptor acceptor_;

public:
    listener(net::io_context& ioc, tcp::endpoint endpoint)
        : ioc_(ioc), acceptor_(ioc) {
        beast::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec) throw std::runtime_error(ec.message());

        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec) throw std::runtime_error(ec.message());

        acceptor_.bind(endpoint, ec);
        if (ec) throw std::runtime_error(ec.message());

        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec) throw std::runtime_error(ec.message());
    }

    void start() { accept(); }

private:
    void accept() {
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(&listener::on_accept, shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<http_session>(std::move(socket))->start();
        }
        accept();
    }
};

int main() {
    try {
        auto const address = net::ip::make_address("127.0.0.1");
        auto const port = static_cast<unsigned short>(8080);
        auto const threads = 4;

        net::io_context ioc{threads};
        std::make_shared<listener>(ioc, tcp::endpoint{address, port})->start();

        std::cout << "Server starting at http://127.0.0.1:8080" << std::endl;

        std::vector<std::thread> v;
        v.reserve(threads - 1);
        for (auto i = threads - 1; i > 0; --i)
            v.emplace_back([&ioc] { ioc.run(); });
        ioc.run();

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}