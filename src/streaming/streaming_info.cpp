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
#include <ctime>

#include "../../include/_tdma_api.h"

#ifdef _WIN32
#define timegm _mkgmtime
#endif

namespace tdma {

using namespace std;

long long
timestamp_to_ms(string ts)
{
    //"2018-06-12T02:18:23+0000"
    if( ts.size() != 24 || ts.substr(20,4) != "0000" )
        throw APIException("invalid timestamp from streamerInfo");

    auto pos = ts.begin();
    tm t = {0};
    t.tm_year = stoi(string(pos,pos + 4)) - 1900;
    pos += 5;
    t.tm_mon = stoi(string(pos, pos + 2)) - 1;
    pos += 3;
    t.tm_mday = stoi(string(pos, pos + 2));
    pos += 3;
    t.tm_hour = stoi(string(pos, pos + 2));
    pos += 3;
    t.tm_min = stoi(string(pos, pos + 2));
    pos += 3;
    t.tm_sec = stoi(string(pos, pos + 2));
    t.tm_isdst = 0;
    return timegm(&t) * 1000LL;
}


// TODO allow to search multiple accounts; for now just default to first
StreamerInfo
get_streamer_info(Credentials& creds)
{
    json j = get_user_principals_for_streaming(creds);

    auto i_acct = j.find("accounts");
    if( i_acct == j.end() )
        throw APIException("returned user principals has no 'accounts'");

    auto i_sinfo = j.find("streamerInfo");
    if( i_sinfo == j.end() )
        throw APIException("returned user principals has no 'streamerInfo");

    json acct = (*i_acct)[0];
    json sinfo = *i_sinfo;

    StreamerInfo si;
    try{
        si.credentials.user_id = acct.at("accountId");
        si.credentials.token= sinfo.at("token");
        si.credentials.company = acct.at("company");
        si.credentials.segment = acct.at("segment");
        si.credentials.cd_domain = acct.at("accountCdDomainId");
        si.credentials.user_group = sinfo.at("userGroup");
        si.credentials.access_level = sinfo.at("accessLevel");
        si.credentials.authorized = true;
        si.credentials.timestamp = timestamp_to_ms(sinfo.at("tokenTimestamp"));
        si.credentials.app_id = sinfo.at("appId");
        si.credentials.acl = sinfo.at("acl");
        string addr = sinfo.at("streamerSocketUrl");
        si.url = "wss://" + addr + "/ws";
        si.primary_acct_id = j.at("primaryAccountId");
        si.encode_credentials();
    }catch(json::exception& e){
        throw APIException("failed to convert UserPrincipals JSON to"
                           " StreamerInfo: " + string(e.what()));
    }

    return si;
}


void
StreamerInfo::encode_credentials()
{
    stringstream ss;
    ss<< "userid" << "=" << credentials.user_id << "&"
      << "token" << "=" << credentials.token << "&"
      << "company" << "=" << credentials.company << "&"
      << "segment" << "=" << credentials.segment << "&"
      << "cddomain" << "=" << credentials.cd_domain << "&"
      << "usergroup" << "=" << credentials.user_group << "&"
      << "accesslevel" << "=" << credentials.access_level << "&"
      << "authorized" << "=" << (credentials.authorized ? "Y" : "N") << "&"
      << "acl" << "=" << credentials.acl << "&"
      << "timestamp" << "=" << credentials.timestamp << "&"
      << "appid" << "=" << credentials.app_id;

    credentials_encoded = util::url_encode(ss.str());
}


StreamerService::StreamerService(string service_name)
{
    if( service_name == "NONE" )
        _service = type::NONE;
    else if( service_name == "ADMIN" )
        _service = type::ADMIN;
    else if( service_name == "ACTIVES_NASDAQ" )
        _service = type::ACTIVES_NASDAQ;
    else if( service_name == "ACTIVES_NYSE" )
        _service = type::ACTIVES_NYSE;
    else if( service_name == "ACTIVES_OTCBB" )
        _service = type::ACTIVES_OTCBB;
    else if( service_name == "ACTIVES_OPTIONS" )
        _service = type::ACTIVES_OPTIONS;
    else if( service_name == "CHART_EQUITY" )
        _service = type::CHART_EQUITY;
    //else if( service_name == "CHART_FOREX" )
    //    _service = type::CHART_FOREX;
    else if( service_name == "CHART_FUTURES" )
        _service = type::CHART_FUTURES;
    else if( service_name == "CHART_OPTIONS" )
        _service = type::CHART_OPTIONS;
    else if( service_name == "QUOTE" )
        _service = type::QUOTE;
    else if( service_name == "LEVELONE_FUTURES" )
        _service = type::LEVELONE_FUTURES;
    else if( service_name == "LEVELONE_FOREX" )
        _service = type::LEVELONE_FOREX;
    else if( service_name == "LEVELONE_FUTURES_OPTIONS" )
        _service = type::LEVELONE_FUTURES_OPTIONS;
    else if( service_name == "OPTION" )
        _service = type::OPTION;
    else if( service_name == "NEWS_HEADLINE" )
        _service = type::NEWS_HEADLINE;
    else if( service_name == "TIMESALE_EQUITY" )
        _service = type::TIMESALE_EQUITY;
    else if( service_name == "TIMESALE_FUTURES" )
        _service = type::TIMESALE_FUTURES;
    //else if( service_name == "TIMESALE_FOREX" )
    //    _service = type::TIMESALE_FOREX;
    else if( service_name == "TIMESALE_OPTIONS" )
        _service = type::TIMESALE_OPTIONS;
    else
        throw ValueException("invalid service name: " + service_name);
}



} /* tdma */
