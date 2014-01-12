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

#ifndef MONGOOSE_WRAPPER_HH
#define MONGOOSE_WRAPPER_HH

///////////////////////////////////////////////////////////////////////////////

#include "mongoose.h"

#include <boost/function.hpp>
#include <boost/lexical_cast.hpp>

#include <cstddef>
#include <cassert>

#include <map>
#include <vector>
#include <string>
#include <stdexcept>

///////////////////////////////////////////////////////////////////////////////

namespace mongoose
{
    namespace detail
    {
        template <typename T>
        class invariants_checker
        {
        public:
            explicit invariants_checker(const T& object)
                : object_(&object)
            {
                object_->check_invariants();
            }
            ~invariants_checker()
            {
                object_->check_invariants();
            }

        private:
            const T* object_;
        };
    }

    namespace util
    {
        template <typename T>
        T* c_array(std::vector<T>& v)
        {
            return v.empty() ? 0 : &v[0];
        }

        template <typename T>
        const T* const_c_array(const std::vector<T>& v)
        {
            return v.empty() ? 0 : &v[0];
        }
    }

    typedef std::vector<char> buffer;

    class passwords_file
    {
    public:
        passwords_file(mg_context* context, const std::string& path);

        bool add_or_replace(const std::string& user, const std::string& password);
        bool remove(const std::string& user);

    private:
        mg_context* context_;
        std::string path_;
    };

    class option_not_exist : public std::runtime_error
    {
    public:
        explicit option_not_exist(const std::string& message)
            : std::runtime_error(message)
        { }
    };

    class option_already_exist : public std::runtime_error
    {
    public:
        explicit option_already_exist(const std::string& message)
            : std::runtime_error(message)
        { }
    };

    class options
    {
    public:
        options();
        ~options();

        void add(const std::string& name, const std::string& value);    // throw(option_already_exist)
        void remove(const std::string& name);
        void clear();

        // mongoose interoperability
        const char** to_c_string_array() const;

    private:
        options(const options&);
        options& operator=(const options&);

        friend class detail::invariants_checker<options>;

        void check_invariants() const
        {
            assert(data_.size() > 0);       // null termination
            assert(data_.size() % 2 != 0);  // n-pairs + null termination
        }

    private:
        typedef std::vector<const char*> data;
        mutable data data_;
    };

    class event_type
    {
    public:
        enum value_type
        {
            new_request = MG_NEW_REQUEST,
            http_error  = MG_HTTP_ERROR,
            event_log   = MG_EVENT_LOG,
            init_ssl    = MG_INIT_SSL,
        };

        event_type(value_type value)
            : value_(value)
        { }

        operator value_type() const
        {
            return value_;
        }

    private:
        value_type value_;
    };

    class tcp_connection
    {
    public:
        explicit tcp_connection(mg_connection* connection);

        std::size_t write(const void* data, std::size_t size);
        std::size_t read(void* data, std::size_t size);

    private:
        mg_connection* connection_;
    };

    typedef std::multimap<std::string, std::string> queries;
    typedef std::multimap<std::string, std::string> cookies;
    typedef std::multimap<std::string, std::string> headers;

    class web_request
    {
    public:
        web_request(mg_connection* connection, const mg_request_info* request_info);

        std::string get_uri() const;
        std::string get_method() const;
        std::string get_query_string() const;   // url decoded

        queries get_queries();
        cookies get_cookies() const;
        headers get_headers() const;

        std::string get_log_message() const;

        std::string get_remote_user() const;
        unsigned long get_remote_ip() const;    // ip (v4) in host-byte-order
        unsigned short get_remote_port() const; // port in host-byte-order

        std::string get_http_version() const;
        int get_status_code() const;

        bool is_ssl() const;

    private:
        mg_connection*   connection_;
        const mg_request_info* request_info_;
    };

    class web_response
    {
    public:
        explicit web_response(tcp_connection connection);

        void set_status_line(const std::string& http_version, int status_code, const std::string& reason_phrase);

        void add_header(const std::string& name, const std::string& value);

        template <typename T>
        void add_header(const std::string& name, const T& value)        // throw(boost::bad_lexical_cast)
        {
            add_header(name, boost::lexical_cast<std::string>(value));
        }

        void write(const void* data, std::size_t size);

        void send();

    private:
        tcp_connection connection_;
        std::string    status_line_;
        headers        headers_;
        buffer         buffer_;
    };

    class web_server_error : public std::runtime_error
    {
    public:
        explicit web_server_error(const std::string& message)
            : std::runtime_error(message)
        { }
    };

    class web_server
    {
    public:
        typedef boost::function<bool (event_type, tcp_connection, web_request)> request_handler;

        web_server();                                                   // throw(web_server_error)
        explicit web_server(const options& options);                    // throw(web_server_error)
        explicit web_server(request_handler handler);                   // throw(web_server_error)
        web_server(const options& options, request_handler handler);    // throw(web_server_error)
        ~web_server();

        std::string get_option(const std::string& name) const;          // throw(option_not_exist)

        passwords_file get_passwords_file(const std::string& path);

    private:
        web_server(const web_server&);
        web_server& operator=(const web_server&);

        static void* on_request(mg_event event, mg_connection* connection, const mg_request_info* request_info);
        bool on_request(event_type type, tcp_connection connection, web_request request);

    private:
        mg_context*     context_;
        request_handler handler_;
    };
}

///////////////////////////////////////////////////////////////////////////////

#endif
