/////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2010-2012 Piotr Likus
// Name:        mongcpp.cpp
// Project:     mongoose
// Purpose:     C++ wrapper for mongoose.
// Author:      Piotr Likus
// Modified by:
// Created:     15/12/2010
// Licence:     MIT
/////////////////////////////////////////////////////////////////////////////


#include <cctype>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <iostream>

#include "mongoose_wrapper.hh"

#include <boost/format.hpp>



///////////////////////////////////////////////////////////////////////////////

using namespace mongoose;
using namespace mongoose::util;
using namespace mongoose::detail;

///////////////////////////////////////////////////////////////////////////////

namespace
{
    const char* CRLF  = "\r\n";

    const char* alloc_c_string(const std::string& s)
    {
    #if defined(_WIN32)
        #pragma warning (disable : 4996)
    #endif

        char* p = new char[s.size() + 1];
        std::strcpy(p, s.c_str());
        return p;
    }

    void free_c_string(const char* p)
    {
        delete[] p;
    }

    std::string to_string(const char* p)
    {
        return p ? p : "";
    }

    class buffer_writer
    {
    public:
        explicit buffer_writer(buffer& buffer)
            : buffer_(buffer)
        { }

        void write(const void* data, std::size_t size)
        {
            std::size_t i = buffer_.size();

            buffer_.resize(i + size);

            std::memcpy(c_array(buffer_) + i, data, size);
        }

    private:
        buffer_writer(const buffer_writer&);
        buffer_writer& operator=(const buffer_writer&);

    private:
        buffer& buffer_;
    };

    // see: http://tools.ietf.org/html/rfc3986
    std::string url_decode(const std::string& s, bool is_form_url_encoded)
    {
        std::ostringstream oss;

        for (std::size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '%')
            {
                // % followed by to hex digits
                assert(i + 2 < s.size());
                assert(std::isxdigit(s[i+1]));
                assert(std::isxdigit(s[i+2]));

                char a = s[i+1];
                a = static_cast<char>(std::tolower(a));
                a = std::isdigit(a) ? a - '0' : a - 'W';

                char b = s[i+2];
                b = static_cast<char>(std::tolower(b));
                b = std::isdigit(b) ? b - '0' : b - 'W';

                char ch = (a << 4) | b;

                oss << ch;

                i += 2;
            }
            else if (is_form_url_encoded && s[i] == '+')
            {
                oss << ' ';
            }
            else
            {
                oss << s[i];
            }
        }

        return oss.str();
    }
}

///////////////////////////////////////////////////////////////////////////////

passwords_file::passwords_file(mg_context* context, const std::string& path)
    : context_(context)
    , path_(path)
{
}

bool passwords_file::add_or_replace(const std::string& user, const std::string& password)
{
    return mg_modify_passwords_file(context_, path_.c_str(), user.c_str(), password.c_str()) != 0;
}

bool passwords_file::remove(const std::string& user)
{
    return mg_modify_passwords_file(context_, path_.c_str(), user.c_str(), 0) != 0;
}

///////////////////////////////////////////////////////////////////////////////

options::options()
{
    data_.push_back(0); // null termination
}

options::~options()
{
    clear();
}

void options::add(const std::string& name, const std::string& value)
{
    invariants_checker<options> chk(*this);

    std::size_t count = data_.size() - 1;

    for (std::size_t index = 0; index < count; index += 2)
    {
        if (name.compare(data_[index]) == 0)
        {
            throw option_already_exist(str(boost::format("an option named \'%1%\' does already exist") % name));
        }
    }

    data_.insert(data_.insert(data_.end() - 1, alloc_c_string(value)), alloc_c_string(name));
}

void options::remove(const std::string& name)
{
    invariants_checker<options> chk(*this);

    std::size_t count = data_.size() - 1;

    for (std::size_t index = 0; index < count; index += 2)
    {
        if (name.compare(data_[index]) == 0)
        {
            free_c_string(data_[index]);
            free_c_string(data_[index + 1]);
            data_.erase(data_.erase(data_.begin() + index));
            break;
        }
    }
}

void options::clear()
{
    invariants_checker<options> chk(*this);

    std::size_t count = data_.size() - 1;

    for (std::size_t index = 0; index < count; ++index)
    {
        free_c_string(data_[index]);
    }

    data_.erase(data_.begin(), data_.end() - 1);
}

const char** options::to_c_string_array() const
{
    invariants_checker<options> chk(*this);

    return c_array(data_);
}

///////////////////////////////////////////////////////////////////////////////

tcp_connection::tcp_connection(mg_connection* connection)
    : connection_(connection)
{
    assert(connection);
}

std::size_t tcp_connection::write(const void* data, std::size_t size)
{
    return mg_write(connection_, data, size);
}

std::size_t tcp_connection::read(void* data, std::size_t size)
{
    return mg_read(connection_, data, size);
}

///////////////////////////////////////////////////////////////////////////////

web_request::web_request(mg_connection* connection, const mg_request_info* request_info)
    : connection_(connection)
    , request_info_(request_info)
{
    assert(request_info);
}

std::string web_request::get_uri() const
{
    return to_string(request_info_->uri);
}

std::string web_request::get_method() const
{
    return to_string(request_info_->request_method);
}

std::string web_request::get_query_string() const
{
    return url_decode(to_string(request_info_->query_string), true);
}


char reqbuff[8096];

queries web_request::get_queries()
{
    queries m;

    std::stringstream iss(get_query_string());
    if( get_method() == "POST" )
    {
        const char* h = mg_get_header(connection_, "Content-Length");
        if( h != NULL )
        {
            int len = ::atoi(h);
            char *buff = new char[len+16];
            if( buff == NULL )
                std::cout << "Bad alloc!!!" << std::endl;
            mg_read(connection_, buff, len );
            buff[len] = 0;
            iss << "&" << url_decode(to_string(buff), true);
            delete [] buff;
            /*mg_read(connection_, reqbuff, len );
            reqbuff[len] = 0;
            iss << url_decode(to_string(reqbuff), true);
            std::cout << "POST detected, len=" << len << ", content:" << iss.str() << std::endl;*/
        }
    }

    std::string s;

    while (std::getline(iss, s, '&'))
    {
        std::size_t i = s.find('=');
        if (i != std::string::npos)
        {
            m.insert(queries::value_type(s.substr(0, i), s.substr(i + 1)));
        }
    }


    return m;
}

cookies web_request::get_cookies() const
{
    cookies m;

    const char* h = mg_get_header(connection_, "Cookie");
    if (h)
    {
        std::istringstream iss(h);

        std::string s;

        while (std::getline(iss, s, ';'))
        {
            std::size_t i = s.find('=');
            if (i != std::string::npos)
            {
                std::string v;
                if (i + 1 < s.size())
                {
                    v = s.substr(i + 1);

                    if ( ! v.empty() && v[0] == '\"')
                    {
                        v.erase(0, 1);
                    }

                    if ( ! v.empty() && v[v.size() - 1] == '\"')
                    {
                        v.erase(v.size() - 1, 1);
                    }
                }

                m.insert(cookies::value_type(s.substr(0, i), v));
            }

            char c;
            while (iss.get(c))
            {
                if (c != ' ')
                {
                    iss.putback(c);
                    break;
                }
            }
        }
    }

    return m;
}


headers web_request::get_headers() const
{
    headers m;

    for (int index = 0; index < request_info_->num_headers; ++index)
    {
        m.insert(headers::value_type(request_info_->http_headers[index].name,
                                     request_info_->http_headers[index].value));
    }

    return m;
}

std::string web_request::get_log_message() const
{
    return to_string(request_info_->log_message);
}

std::string web_request::get_remote_user() const
{
    return to_string(request_info_->remote_user);
}

unsigned long web_request::get_remote_ip() const
{
    return request_info_->remote_ip;
}

unsigned short web_request::get_remote_port() const
{
    return static_cast<unsigned short>(request_info_->remote_port);
}

std::string web_request::get_http_version() const
{
    return to_string(request_info_->http_version);
}

int web_request::get_status_code() const
{
    return request_info_->status_code;
}

bool web_request::is_ssl() const
{
    return request_info_->is_ssl != 0;
}

///////////////////////////////////////////////////////////////////////////////

web_response::web_response(tcp_connection connection)
    : connection_(connection)
{
}

void web_response::set_status_line(const std::string& http_version, int status_code, const std::string& reason_phrase)
{
    std::ostringstream oss;
    oss << "HTTP/" << http_version << " " << status_code << " " << reason_phrase << CRLF;
    status_line_ = oss.str();
}

void web_response::add_header(const std::string& name, const std::string& value)
{
    headers_.insert(headers::value_type(name, value));
}

void web_response::write(const void* data, std::size_t size)
{
    buffer_writer writer(buffer_);
    writer.write(data, size);
}

void web_response::send()
{
    std::ostringstream oss;

    oss << status_line_;

    headers::const_iterator it = headers_.begin();
    headers::const_iterator end = headers_.end();
    for ( ; it != end; ++it)
    {
        oss << it->first << ": " << it->second << CRLF;
    }

    oss << CRLF;

    std::string s = oss.str();

    connection_.write(s.data(), s.size());
    connection_.write(const_c_array(buffer_), buffer_.size());
}

///////////////////////////////////////////////////////////////////////////////

web_server::web_server()
{
    context_ = mg_start(0, 0, 0);
    if ( ! context_)
    {
        throw web_server_error("cannot start web server");
    }
}

web_server::web_server(const options& options)
{
    context_ = mg_start(0, 0, options.to_c_string_array());
    if ( ! context_)
    {
        throw web_server_error("cannot start web server");
    }
}

web_server::web_server(request_handler handler)
    : handler_(handler)
{
    context_ = mg_start(&web_server::on_request, this, 0);
    if ( ! context_)
    {
        throw web_server_error("cannot start web server - check log messages");
    }
}

web_server::web_server(const options& options, request_handler handler)
    : handler_(handler)
{
    context_ = mg_start(&web_server::on_request, this, options.to_c_string_array());
    if ( ! context_)
    {
        throw web_server_error("cannot start web server - check log messages");
    }
}

web_server::~web_server()
{
    mg_stop(context_);
}

std::string web_server::get_option(const std::string& name) const
{
    const char* option = mg_get_option(context_, name.c_str());
    if ( ! option)
    {
        throw option_not_exist(str(boost::format("an option named \'%1%\' does not exist") % name));
    }
    return option;
}

passwords_file web_server::get_passwords_file(const std::string& path)
{
    return passwords_file(context_, path);
}

void* web_server::on_request(mg_event event, mg_connection* connection, const mg_request_info* request_info)
{
    web_server* instance = static_cast<web_server*>(request_info->user_data);

    assert(instance);

    bool processed = instance->on_request(event_type(static_cast<event_type::value_type>(event)), tcp_connection(connection), web_request(connection, request_info));

    return processed ? (void*)1 : (void*)0;
}

bool web_server::on_request(event_type type, tcp_connection connection, web_request request)
{
    assert(handler_);

    return handler_(type, connection, request);
}

///////////////////////////////////////////////////////////////////////////////
