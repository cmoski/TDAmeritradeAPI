/*
Copyright (C) 2018 Jonathon Ogden <jeog.dev@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see http://www.gnu.org/licenses.
*/

#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <iostream>

#include "../include/curl_connect.h"

namespace conn{

using namespace std;


CurlConnection::Init CurlConnection::_init;

CurlConnection::CurlConnection(string url)
    :
        CurlConnection()
    {        
        SET_url(url);
    }


CurlConnection::CurlConnection()
    :
        _header(nullptr),
        _handle( curl_easy_init() )
    {
        set_option(CURLOPT_NOSIGNAL, 1L);
    }


CurlConnection::~CurlConnection()
    {
        close();
    }


size_t
CurlConnection::WriteCallback::write( char* input,
                                      size_t sz,
                                      size_t n,
                                      void* output )
{
    stringbuf& buf = ((WriteCallback*)output)->_buf;
    streamsize ssz = buf.sputn(input, sz*n);
    assert( ssz >= 0 );
    return ssz;
}


tuple<long, string, clock_ty::time_point>
CurlConnection::execute()
{
    if( !_handle )
        throw CurlException("connection/handle has been closed");

    WriteCallback cb;
    set_option(CURLOPT_WRITEFUNCTION, &WriteCallback::write);
    set_option(CURLOPT_WRITEDATA, &cb);

    CURLcode ccode = curl_easy_perform(_handle);
    auto tp = clock_ty::now();
    if( ccode != CURLE_OK ){
        throw CurlConnectionError(ccode);
    }

    string res = cb.str();
    cb.clear();
    long c;
    curl_easy_getinfo(_handle, CURLINFO_RESPONSE_CODE, &c);

    return make_tuple(c, res, tp);
}


void
CurlConnection::close()
{
    if( _header ){
        curl_slist_free_all(_header);
        _header = nullptr;
    }
    if( _handle ){
        curl_easy_cleanup(_handle);
        _handle = nullptr;
    }
    _options.clear(); 
}

void
CurlConnection::SET_url(std::string url)
{ set_option(CURLOPT_URL, url.c_str()); }

void
CurlConnection::SET_ssl_verify()
{
    set_option(CURLOPT_SSL_VERIFYPEER, 1L);
    set_option(CURLOPT_SSL_VERIFYHOST, 2L);   
}

void
CurlConnection::SET_ssl_verify_using_ca_bundle(string path)
{
    SET_ssl_verify();
    set_option(CURLOPT_CAINFO, path.c_str());
}

void
CurlConnection::SET_ssl_verify_using_ca_certs(string dir)
{
    SET_ssl_verify();
    set_option(CURLOPT_CAPATH, dir.c_str());
}

void
CurlConnection::SET_encoding(string enc)
{ set_option(CURLOPT_ACCEPT_ENCODING, enc.c_str()); }


void
CurlConnection::SET_keepalive()
{ set_option(CURLOPT_TCP_KEEPALIVE, 1L); }


/* CAREFUL */
void
CurlConnection::ADD_headers(const vector<pair<string,string>>& headers)
{
    if( !_handle )
        throw CurlException("connection/handle has been closed");
    
    if( headers.empty() )
        return;    

    for(auto& h : headers){
        string s = h.first + ": " + h.second;
        _header = curl_slist_append(_header, s.c_str());
        if( !_header ){
            throw CurlOptionException("curl_slist_append failed trying to "
                                      "add header", CURLOPT_HEADER, s);
        }
    }

    return set_option(CURLOPT_HTTPHEADER, _header);
}


void
CurlConnection::RESET_headers()
{
    curl_slist_free_all(_header);
    _header = nullptr;
    _options.erase(CURLOPT_HTTPHEADER);
}

void
CurlConnection::RESET_options()
{
    RESET_headers();
    if(_handle){
        curl_easy_reset(_handle);
    }    
    _options.clear();
}


const std::string HTTPSConnection::DEFAULT_ENCODING("gzip");

HTTPSConnection::HTTPSConnection()
    :
        CurlConnection()
    {
        SET_ssl_verify();
    }

HTTPSConnection::HTTPSConnection(string url)
    :
        CurlConnection(url)
    {
        SET_ssl_verify();
    }


HTTPSGetConnection::HTTPSGetConnection()
    :
        HTTPSConnection()
    {
        set_option(CURLOPT_HTTPGET, 1L);
        SET_encoding(DEFAULT_ENCODING);
        SET_keepalive();
    }


HTTPSGetConnection::HTTPSGetConnection(string url)
    :
        HTTPSConnection(url)
    {
        set_option(CURLOPT_HTTPGET, 1L);
        SET_encoding(DEFAULT_ENCODING);
        SET_keepalive();
    }


HTTPSPostConnection::HTTPSPostConnection()
    :
        HTTPSConnection()
    {
        set_option(CURLOPT_POST, 1L);
        SET_encoding(DEFAULT_ENCODING);
        SET_keepalive();     
    }

HTTPSPostConnection::HTTPSPostConnection(string url)
    :
        HTTPSConnection(url)
    {
        set_option(CURLOPT_POST, 1L);
        SET_encoding(DEFAULT_ENCODING);
        SET_keepalive();
    }


void
HTTPSPostConnection::SET_fields(const vector<pair<string,string>>& fields)
{
    if( is_closed() )
        throw CurlException("connection/handle has been closed");
    
    /* CURLOPT_POST FIELDS DOES NOT COPY STRING */
    if( !fields.empty() ){
        stringstream ss;
        for(auto& f : fields){
            ss<< f.first << "=" << f.second << "&";
        }
        string s(ss.str());
        if( !s.empty() ){
            assert( s.back() == '&' );
            s.erase(s.end()-1, s.end());
        }
        set_option(CURLOPT_COPYPOSTFIELDS,s.c_str());
    }
}



CurlOptionException::CurlOptionException(CURLoption opt, string val)
    :
        CurlException( "error setting easy curl option ("
                        + to_string(opt) + ") with value ("
                        + val + ")"),
        option(opt),
        value(val)
    {
    }


CurlOptionException::CurlOptionException(string what, CURLoption opt, string val)
    :
        CurlException(what),
        option(opt),
        value(val)
    {
    }


CurlConnectionError::CurlConnectionError(CURLcode code)
    :
        CurlException("connection error (" + to_string(code) + ")"),
        code(code)
    {
    }


vector<pair<string, string>>
fields_str_to_map(const string& fstr)
{
    static const string C{'&'};

    vector<pair<string, string>> res;
    auto b = fstr.cbegin();
    do{
        auto e = find_first_of(b, fstr.cend(), C.cbegin(), C.cend());
        string s(b,e);
        if( !s.empty() ){
            auto s_b = s.cbegin();
            auto s_e = s.cend();
            auto sep = find(s_b, s_e,'=');
            if( sep != s_e ){
                string k(s_b,sep);
                string v(sep+1, s_e);
                res.emplace_back(k,v);
            }
        }
        if( e == fstr.cend() )
            break; // avoid b going past end
        b = e + 1;
    }while(true);

    return res;
}


vector<pair<string, string>>
header_list_to_map(struct curl_slist *hlist)
{
    vector<pair<string, string>> res;
    while( hlist ){
        string s(hlist->data);
        auto i = find(s.cbegin(), s.cend(),':');
        string k(s.cbegin(),i);
        string v(i+1, s.cend());
        res.emplace_back(k, v);
        hlist = hlist->next;
    }
    return res;
}


ostream&
operator<<(ostream& out, CurlConnection& connection)
{
    for( auto& opt : connection.get_option_strings() ){

        auto oiter = CurlConnection::option_strings.find(opt.first);
        if( oiter == CurlConnection::option_strings.end() ){
            out<< "\tUNKNOWN" << endl;
            continue;
        }

        switch(opt.first){
        case CURLOPT_COPYPOSTFIELDS:
            out<< "\t" << oiter->second << ":" << endl;
            for(auto p : fields_str_to_map(opt.second)){
                out<< "\t\t" << p.first << "\t" << p.second << endl;
            }
            continue;
        case CURLOPT_HTTPHEADER:
            out<< "\t" << oiter->second << ":" << endl;
            for(auto p : header_list_to_map(connection._header)){
                out<< "\t\t" << p.first << "\t" << p.second << endl;
            }
            continue;
        case CURLOPT_WRITEDATA:
        case CURLOPT_WRITEFUNCTION:
            out<< "\t" << oiter->second << "\t" << hex
               << stoull(opt.second) << dec << endl;
            continue;
        default:
            out<< "\t" << oiter->second << "\t" << opt.second << endl;
        }
    }

    return out;
}


const map<CURLoption, string> CurlConnection::option_strings = {
    { CURLOPT_SSL_VERIFYPEER, "CURLOPT_SSL_VERIFYPEER"},
    { CURLOPT_SSL_VERIFYHOST, "CURLOPT_SSL_VERIFYHOST"},
    { CURLOPT_CAINFO, "CURLOPT_CAINFO"},
    { CURLOPT_CAPATH, "CURLOPT_CAPATH"},
    { CURLOPT_URL, "CURLOPT_URL"},
    { CURLOPT_ACCEPT_ENCODING, "CURLOPT_ACCEPT_ENCODING"},
    { CURLOPT_TCP_KEEPALIVE, "CURLOPT_TCP_KEEPALIVE"},
    { CURLOPT_HTTPGET, "CURLOPT_HTTPGET"},
    { CURLOPT_POST, "CURLOPT_POST"},
    { CURLOPT_COPYPOSTFIELDS, "CURLOPT_COPYPOSTFIELDS"},
    { CURLOPT_WRITEFUNCTION, "CURLOPT_WRITEFUNCTION"},
    { CURLOPT_WRITEDATA, "CURLOPT_WRITEDATA"},
    { CURLOPT_HTTPHEADER, "CURLOPT_HTTPHEADER"},
    { CURLOPT_NOSIGNAL, "CURLOPT_NOSIGNAL"}
};



} /* conn */
