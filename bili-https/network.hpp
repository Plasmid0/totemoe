
#pragma once

#include "stdafx.hpp"
#include "bili-settings/bili-settings.hpp"
#include <string>
#include <curl/curl.h>

#ifdef _DEBUG
#include <fstream>
#define TURN_ON_DUMPS
#endif

static char errorBuffer[CURL_ERROR_SIZE];
static std::string buffer;

// libcurl write callback function
static int writer(char *data, size_t size, size_t nmemb, std::string *writerData)
{
    if (writerData == NULL)
    {
        return 0;
    }

    writerData->append(data, size*nmemb);

    return size * nmemb;
}

// libcurl write data callback function
size_t writefunction(void *ptr, size_t size, size_t nmemb, void *stream);

// convert json to post fields
static std::string toPostFields(json const &data)
{
    std::string result;

    if (data.is_object())
    {
        for (json::const_iterator it = data.cbegin(); it != data.cend(); ++it)
        {
            if (it != data.cbegin())
            {
                result += "&";
            }
            result += it.key() + "=";
            if (it->is_string())
            {
                result += it.value().get<std::string>();
            }
            else if (it->is_number_integer())
            {
                result += std::to_string(it.value().get<int>());
            }
            else
            {
                std::stoi("ABC");
            }
        }
    }

    return result;
}

#ifdef _DEBUG
static
int my_trace(CURL *handle, curl_infotype type,
    char *data, size_t size,
    void *userp)
{
    const char *text;
    (void)handle; /* prevent compiler warning */
    (void)userp;

    std::ofstream file;
    switch (type) {
    case CURLINFO_TEXT:
        file.open("requests.log", std::ofstream::out | std::ofstream::app);
        file << data << std::endl;
        file.close();
    default: /* in case a new one is introduced to shock us */
        return 0;

    case CURLINFO_HEADER_OUT:
        text = "=> Send header";
        break;
    case CURLINFO_DATA_OUT:
        text = "=> Send data";
        break;
    case CURLINFO_SSL_DATA_OUT:
        text = "=> Send SSL data";
        break;
    case CURLINFO_HEADER_IN:
        text = "<= Recv header";
        break;
    case CURLINFO_DATA_IN:
        text = "<= Recv data";
        break;
    case CURLINFO_SSL_DATA_IN:
        text = "<= Recv SSL data";
        break;
    }

    file.open("requests.log", std::ofstream::out | std::ofstream::app);
    file << text << "\n" << data << std::endl;
    file.close();

    return 0;
}
#endif

// convert json to cookie fields
static std::string toCookieFields(json const &data)
{
    std::string result;

    if (data.is_object())
    {
        for (json::const_iterator it = data.cbegin(); it != data.cend(); ++it)
        {
            if (it != data.cbegin())
            {
                result += "; ";
            }
            result += it.key() + "=";
            if (it->is_string())
            {
                result += it.value().get<std::string>();
            }
            else if (it->is_number_integer())
            {
                result += std::to_string(it.value().get<int>());
            }
            else
            {
                std::stoi("ABC");
            }
        }
    }

    return result;
}

// libcurl connection initialization
static bool initHttpsGet(CURL *&conn, const char *url)
{
    CURLcode code;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    conn = curl_easy_init();

    code = curl_easy_setopt(conn, CURLOPT_SSLCERTTYPE, "PEM");

    code = curl_easy_setopt(conn, CURLOPT_SSL_VERIFYPEER, 1L);
    if (conn == NULL)
    {
        // fprintf(stderr, "Failed to create CURL connection\n");
        // exit(EXIT_FAILURE);
    }

    Bili::Settings::File::Load();
    std::string caPath = Bili::Settings::File::Get("Security", "liveCertificateChain");
    code = curl_easy_setopt(conn, CURLOPT_CAINFO, caPath.c_str());

    code = curl_easy_setopt(conn, CURLOPT_ERRORBUFFER, errorBuffer);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set error buffer [%d]\n", code);
        // return false;
    }

    code = curl_easy_setopt(conn, CURLOPT_URL, url);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set URL [%s]\n", errorBuffer);
        // return false;
    }

    code = curl_easy_setopt(conn, CURLOPT_FOLLOWLOCATION, 1L);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set redirect option [%s]\n", errorBuffer);
        // return false;
    }

    code = curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, writer);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set writer [%s]\n", errorBuffer);
        // return false;
    }

    code = curl_easy_setopt(conn, CURLOPT_WRITEDATA, &buffer);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set write data [%s]\n", errorBuffer);
        // return false;
    }

    return true;
}


static std::string curlHttpsGet(std::string const &api, json const &data = json::object())
{
    bool room_invalid = false;
    std::string result;

    CURL *conn = NULL;
    CURLcode code;

    std::string url = api;
    if (data.is_object())
    {
        if (data.cbegin() != data.cend())
        {
            url += "?";
            url += toPostFields(data);
        }
    }

    if (!initHttpsGet(conn, url.c_str()))
    {
    }
    else
    {
        code = curl_easy_perform(conn);
        if (code != CURLE_OK)
        {
        }
        else
        {
#ifdef TURN_ON_DUMPS
            std::ofstream file;
            file.open("buffer.log");
            file << buffer << std::endl;
            file.close();
#endif
            result = buffer;
        }
        curl_easy_cleanup(conn);
        buffer.clear();
    }

    return result;
}

// libcurl retrieve room data
static json curlHttpsGet(std::string const &api, ROOM roomid, json const &data)
{
    bool room_invalid = false;
    json result;
    json response;

    room_invalid |= roomid <= 0;
    if (!room_invalid)
    {
        CURL *conn = NULL;
        CURLcode code;

        std::string url = api;
        if (data.is_object())
        {
            if (data.cbegin() != data.cend())
            {
                url += "?";
                url += toPostFields(data);
            }
        }

        if (!initHttpsGet(conn, url.c_str()))
        {
            result["error"] = "Connection initialization failed";
        }
        else
        {
            try
            {
                code = curl_easy_perform(conn);
            }
            catch (...)
            {
                code = CURLE_UNSUPPORTED_PROTOCOL;
            }
            if (code != CURLE_OK)
            {
                result["error"] = "Failed to get \"" + url + "\"";
            }
            else
            {
#ifdef TURN_ON_DUMPS
                std::ofstream file;
                file.open("buffer.log");
                file << buffer << std::endl;
                file.close();
#endif
                try
                {
                    response = json::parse(buffer);
                }
                catch (...)
                {
                    response = { { "error", "Cannot parse buffer as JSON."} };
                }
#ifdef TURN_ON_DUMPS
                file.open("dump.log");
                file << response.dump(4) << std::endl;
                file.close();
#endif
                room_invalid |= !(response["code"] == 0);
                room_invalid |= !(response["msg"] == "success" || response["msg"] == "ok" || response["msg"] == "");
            }
            curl_easy_cleanup(conn);
            buffer.clear();
        }
    }

    // If the roomid is invalid, return error code.
    if (room_invalid)
    {
        result = "{\"error\":\"invalid roomid\"}"_json;
    }
    else
    {
        result = response["data"];
    }
#ifdef TURN_ON_DUMPS
    std::ofstream file;
    file.open("result.log");
    file << result.dump(4) << std::endl;
    file.close();
#endif
    return result;
}

// libcurl connection initialization
static bool initHttpsPost(CURL *&conn, const char *url,
    json const &options = json::object())
{
    CURLcode code;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    conn = curl_easy_init();

    code = curl_easy_setopt(conn, CURLOPT_SSLCERTTYPE, "PEM");

    code = curl_easy_setopt(conn, CURLOPT_SSL_VERIFYPEER, 1L);
    if (conn == NULL)
    {
        // fprintf(stderr, "Failed to create CURL connection\n");
        // exit(EXIT_FAILURE);
    }

    Bili::Settings::File::Load();
    std::string caPath = Bili::Settings::File::Get("Security", "liveCertificateChain");
    code = curl_easy_setopt(conn, CURLOPT_CAINFO, caPath.c_str());

    code = curl_easy_setopt(conn, CURLOPT_ERRORBUFFER, errorBuffer);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set error buffer [%d]\n", code);
        // return false;
    }

    code = curl_easy_setopt(conn, CURLOPT_URL, url);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set URL [%s]\n", errorBuffer);
        // return false;
    }

    code = curl_easy_setopt(conn, CURLOPT_POST, 1L);

#ifdef _DEBUG
    code = curl_easy_setopt(conn, CURLOPT_DEBUGFUNCTION, my_trace);

    /* the DEBUGFUNCTION has no effect until we enable VERBOSE */
    code = curl_easy_setopt(conn, CURLOPT_VERBOSE, 1L);
#endif

    /* Now specify the POST data */
    if (options.find("post") != options.end())
    {
        std::string content = options["post"];
        code = curl_easy_setopt(conn, CURLOPT_POSTFIELDSIZE, content.length());
        code = curl_easy_setopt(conn, CURLOPT_COPYPOSTFIELDS, content.c_str());
    }

    if (options.find("cookie") != options.end())
    {
        std::string content = options["cookie"];
        content = "l=v; fts=1440723789; pgv_pvi=4705329152; _qddaz=QD.5g8jak.ir9its.itr7dctx; rpdid=owkmmloqxqdoplqqlwxxw; LIVE_BUVID=1c8e5fa964899eb51d8bcc2a8c977498; LIVE_BUVID__ckMd5=d7b5a404c7d57d77; sid=a571ix2g; UM_distinctid=15e670fde8870-0f95dbeec4947d-e313761-144000-15e670fde8991; uTZ=360; buvid3=FE399F26-4DE9-4C57-AD3D-88FB99A705FC4387infoc; biliMzIsnew=1; biliMzTs=0; finger=edc6ecda; im_seqno_4278633=68403; DedeUserID=4278633; DedeUserID__ckMd5=a185c69d095516bd; SESSDATA=d3411f47%2C1516347349%2Cabb243a5; bili_jct=45d84c2100b3399aaff81c8aaf59075a; Hm_lvt_8a6e55dbd2870f0f5bc9194cddf32a02=1513490311,1513564826,1513724785,1513792102; _dfcaptcha=f34e57f27ca5eb78751e2508445c2d5f; Hm_lpvt_8a6e55dbd2870f0f5bc9194cddf32a02=1513802426";
        code = curl_easy_setopt(conn, CURLOPT_COOKIE, content.c_str());
    }

    code = curl_easy_setopt(conn, CURLOPT_FOLLOWLOCATION, 1L);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set redirect option [%s]\n", errorBuffer);
        // return false;
    }

    code = curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, writer);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set writer [%s]\n", errorBuffer);
        // return false;
    }

    code = curl_easy_setopt(conn, CURLOPT_WRITEDATA, &buffer);
    if (code != CURLE_OK)
    {
        // fprintf(stderr, "Failed to set write data [%s]\n", errorBuffer);
        // return false;
    }

    return true;
}

// libcurl retrieve room data
static json curlHttpsPost(std::string const &api, ROOM roomid,
    json const &options = json::object())
{
    bool room_invalid = false;
    json stringOptions;
    json result;
    json response;

    room_invalid |= roomid <= 0;
    if (!room_invalid)
    {
        CURL *conn = NULL;
        CURLcode code;

        std::string url = api;

        if (options.find("post") != options.end())
        {
            if (options["post"].is_object())
            {
                std::string content = toPostFields(options["post"]["content"]);
                bool escape = options["post"].find("escape") != options["post"].end();
                if (escape)
                {
                    char *output = curl_easy_escape(conn, content.c_str(), content.length());
                    if (output)
                    {
                        content.assign(output);
                        curl_free(output);
                    }
                }
                stringOptions["post"] = content;
            }
            else
            {
                stringOptions["post"] = options["post"].dump();
            }
        }

        if (options.find("cookie") != options.end())
        {
            if (options["cookie"].is_object())
            {
                std::string content = toCookieFields(options["cookie"]["content"]);
                bool escape = options["cookie"].find("escape") != options["cookie"].end();
                if (escape)
                {
                    char *output = curl_easy_escape(conn, content.c_str(), content.length());
                    if (output)
                    {
                        content.assign(output);
                        curl_free(output);
                    }
                }
                stringOptions["cookie"] = content;
            }
            else
            {
                stringOptions["cookie"] = options["cookie"].dump();
            }
        }

        /*
        std::string postFields = toPostFields(data);
        std::string cookieFields = toCookieFields(cookie);

        json data, cookie;

        if (options.find("data") != options.end())
        {
            std::string content = options["data"];
            char *postField = curl_easy_escape(conn, postFields.c_str(), postFields.length());
        }
        */

        if (!initHttpsPost(conn, url.c_str(), stringOptions))
        {
            result["error"] = "Connection initialization failed";
        }
        else
        {
            code = curl_easy_perform(conn);
            if (code != CURLE_OK)
            {
                result["error"] = "Failed to get \"" + url + "\"";
            }
            else
            {
#ifdef TURN_ON_DUMPS
                std::ofstream file;
                file.open("buffer.log");
                file << buffer << std::endl;
                file.close();
#endif
                response = json::parse(buffer);
#ifdef TURN_ON_DUMPS
                file.open("dump.log");
                file << response.dump(4) << std::endl;
                file.close();
#endif
                room_invalid |= !(response["code"] == 0);
                room_invalid |= !(response["msg"] == "success" || response["msg"] == "ok" || response["msg"] == "");
            }
            curl_easy_cleanup(conn);
            buffer.clear();
        }
    }

    // If the roomid is invalid, return error code.
    if (room_invalid)
    {
        result = "{\"error\":\"invalid roomid\"}"_json;
    }
    else
    {
        result = response["data"];
    }
#ifdef TURN_ON_DUMPS
    std::ofstream file;
    file.open("result.log");
    file << result.dump(4) << std::endl;
    file.close();
#endif
    return result;
}