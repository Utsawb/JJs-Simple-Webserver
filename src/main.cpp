/*
 *  Simple example of routes and an image
 */


#include <fstream>

#include "http.hpp"

jj::http_response about(jj::http_request request)
{
    jj::http_response response;

    // Get the html file
    std::ifstream f("web_files/index.html");

    response.version = "HTTP/1.1";
    response.status_code = 200;
    response.status_message = "Success";
    response.body = std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    response.headers.insert_or_assign("Content-Length", std::to_string(response.body.size()));
    response.headers.insert_or_assign("Content-Type", "text/html");

    return response;
}

jj::http_response image_request(jj::http_request request)
{
    jj::http_response response;

    // Get the img file
    std::ifstream f("web_files/mars-5k_1538069567.jpg.webp");

    response.version = "HTTP/1.1";
    response.status_code = 200;
    response.status_message = "Success";
    response.body = std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    response.headers.insert_or_assign("Content-Length", std::to_string(response.body.size()));
    response.headers.insert_or_assign("Content-Type", "image/webp");

    return response;
}

int main(void)
{
    jj::http_server server("127.0.0.1", 25565);

    // Add routes
    server.routes["/"] = about;
    server.routes["/img.png"] = image_request;

    server.start_listen();

    return 0;
}
