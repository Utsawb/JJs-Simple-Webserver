#pragma once

#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

volatile std::sig_atomic_t active;
void sigint_handle(int)
{
    std::cout << "\nCTRL-C Detected, Exiting\n";
    exit(0);
}

namespace jj
{
    enum http_method
    {
        GET,
        HEAD,
        POST,
        PUT,
        DELETE,
        CONNECT,
        OPTIONS,
        TRACE,
        PATCH
    };
    static const char *http_method_str[] = {"GET",     "HEAD",    "POST",  "PUT",  "DELETE",
                                            "CONNECT", "OPTIONS", "TRACE", "PATCH"};

    http_method parse_verb(const std::string &method)
    {
        for (std::size_t i = 0; i < 8; ++i)
        {
            if (method == http_method_str[i])
            {
                return static_cast<http_method>(i);
            }
        }

        throw std::runtime_error("Unknown http method");
    }
    const char *parse_verb(const http_method &method)
    {
        return http_method_str[method];
    }

    struct http_request
    {
            http_method method;
            std::string path;
            std::string version;
            std::map<std::string, std::string> headers;
            std::string body;

            http_request(const char *buffer)
            {
                std::stringstream ss(buffer);
                std::string temp;
                std::getline(ss, temp, ' ');
                this->method = parse_verb(temp.c_str());
                std::getline(ss, this->path, ' ');
                std::getline(ss, this->version, '\n');

                while (temp != "\n" && std::getline(ss, temp))
                {
                    this->headers.insert_or_assign(temp.substr(0, temp.find(':')),
                                                   temp.substr(temp.find(':') + 1, temp.length() - 1));
                }

                std::getline(ss, this->body);
            }
    };

    struct http_response
    {
            std::string version;
            uint32_t status_code;
            std::string status_message;
            std::map<std::string, std::string> headers;
            std::string body;

            std::string create_response()
            {
                std::stringstream ss;
                ss << this->version << " " << this->status_code << " " << this->status_message << std::endl;
                for (const auto &[key, value] : this->headers)
                {
                    ss << key << ": " << value << '\n';
                }
                ss << std::endl;
                ss << this->body;

                return ss.str();
            }
    };

    class http_server
    {
        private:
            static constexpr uint16_t default_port = 80;
            static constexpr char default_ip[] = "0.0.0.0";
            static constexpr std::size_t buffer_size = 1 << 15;
            volatile std::sig_atomic_t active;

            typedef http_response (*route)(http_request);

            std::string server_ip;
            uint16_t listening_port;

            int32_t listening_socket;
            int32_t transmitting_socket;

            char receive_buffer[buffer_size];

            struct sockaddr_in socket_address;
            uint32_t socket_address_length;

            void start_server()
            {
                struct sigaction a;
                a.sa_handler = sigint_handle;
                a.sa_flags = 0;
                sigemptyset(&a.sa_mask);
                sigaction(SIGINT, &a, NULL);

                this->listening_socket = socket(AF_INET, SOCK_STREAM, 0);
                if (this->listening_socket < 0)
                {
                    throw std::runtime_error("Failed to create socket");
                }

                this->socket_address.sin_family = AF_INET;
                this->socket_address.sin_port = htons(this->listening_port);
                this->socket_address.sin_addr.s_addr = inet_addr(this->server_ip.c_str());
                this->socket_address_length = sizeof(this->socket_address);

                if (bind(this->listening_socket, (sockaddr *)&this->socket_address, this->socket_address_length) < 0)
                {
                    throw std::runtime_error("Failed to bind to: " + this->server_ip + ":" +
                                             std::to_string(this->listening_port));
                }
            }

        public:
            std::map<std::string, route> routes;
            http_server(const std::string &ip = default_ip, const uint16_t &port = default_port)
                : server_ip(ip), listening_port(port)
            {
                this->start_server();
            }
            ~http_server()
            {
            }
            void start_listen()
            {
                if (listen(this->listening_socket, 32) < 0)
                {
                    throw std::runtime_error("Failed to start listen on: " + this->server_ip + ":" +
                                             std::to_string(this->listening_port));
                }
                std::cout << "Started listening on: " << this->server_ip << ":" << std::to_string(this->listening_port)
                          << '\n';

                active = true;
                while (active)
                {
                    this->transmitting_socket =
                        accept(this->listening_socket, (sockaddr *)&this->socket_address, &this->socket_address_length);
                    if (active == false)
                    {
                        break;
                    }
                    if (this->transmitting_socket < 0 && active == true)
                    {
                        throw std::runtime_error("Failled to accept requests");
                    }
                    std::size_t received_number_bytes =
                        read(this->transmitting_socket, this->receive_buffer, this->buffer_size);
                    this->receive_buffer[received_number_bytes] = '\0';
                    if (received_number_bytes < 0)
                    {
                        throw std::runtime_error("Failed to read from socket");
                    }

                    // Routing
                    http_request request(this->receive_buffer);
                    http_response response;
                    try
                    {
                        response = routes.at(request.path)(request);
                    }
                    catch (...)
                    {
                        response.version = "HTTP/1.1";
                        response.status_code = 404;
                        response.status_message = "Not Found";
                        response.body = "Page not found";
                        response.headers.insert_or_assign("Content-Length", std::to_string(response.body.size()));
                        response.headers.insert_or_assign("Content-Type", "text/html");
                    }

                    std::string raw_response = response.create_response();
                    write(this->transmitting_socket, raw_response.data(), raw_response.size());

                    close(this->transmitting_socket);
                }

                close(this->listening_port);
                std::cout << "Ending listener" << std::endl;
            }
    };
} // namespace jj
