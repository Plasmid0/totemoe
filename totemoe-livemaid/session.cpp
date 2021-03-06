
#include "stdafx.h"
#include "session.hpp"
#include <algorithm>
#include <codecvt>
#include <ctime>
#include <regex>

MessageSession::MessageSession() :
    m_bAutoReconnect(true), m_bStopThread(true), m_bLockVScroll(false),
    m_nNextMessageID(0)
{
}

MessageSession::~MessageSession()
{
    if (!isClosed())
    {
        disconnect();
    }
    m_bStopThread = true;

    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

bool MessageSession::isClosed() const
{
    auto status = m_room.getMetaData();
    if (status == nullptr)
    {
        return true;
    }
    else if (status->getStatus() == "Closed")
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool MessageSession::isOpen() const
{
    auto status = m_room.getMetaData();
    if (status == nullptr)
    {
        return false;
    }
    else if (status->getStatus() == "Open")
    {
        return true;
    }
    else
    {
        return false;
    }
}

void MessageSession::join(ROOM room)
{
    m_room.join(room);
}

void MessageSession::connect()
{
    m_room.connect();
    m_bStopThread = false;
    // ============================================ DETATCH A NEW THREAD ==== //
    // ==== The thread will forever attempt to fetch and parse messages. ==== //
    // ==== The thread is joined in the destructor of this class.        ==== //
    // ====================================================================== //
    m_thread = std::thread([this]() {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        auto roomid = getRoomID();

        bool quit = false;
        while (!quit)
        {
            auto messages = m_room.retrieve();
            if (!messages.empty())
            {
                if (m_pStatusBar != nullptr)
                {
                    m_pStatusBar->setText(2, L"MSG (" + std::to_wstring(m_vDisplay.size()) + L")");
                }
                for (auto &msg : messages)
                {
                    parseMessage({
                        { "time" , time(nullptr) },
                        { "room" , m_room.getRoomID() },
                        { "data", json::parse(msg) }
                    });
                }
            }
            if (m_bStopThread)
            {
                quit = true;
            }
            using namespace std::chrono_literals;
            std::this_thread::sleep_for(50ms);
        }
    });
    if (m_thread.joinable())
    {
        m_thread.detach();
    }
}

void MessageSession::reconnect()
{
    disconnect();
    connect();
}

void MessageSession::disconnect()
{
    m_bStopThread = true;
    m_room.disconnect(websocketpp::close::status::normal, "User disconnect");
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    std::condition_variable condition;
    std::unique_lock<std::mutex> lock(m_mutex_message);
    condition.wait(lock, [this]() {
        auto const &messages = m_room.retrieve();
        if (!messages.empty())
        {
            if (m_pStatusBar != nullptr)
            {
                m_pStatusBar->setText(2, L"MSG (" + std::to_wstring(m_vDisplay.size()) + L")");
            }
            for (auto &msg : messages)
            {
                parseMessage({
                    { "time" , time(nullptr) },
                    { "room" , m_room.getRoomID() },
                    { "data", json::parse(msg) }
                });
            }
        }
        return !messages.empty();
    });
}

void MessageSession::setFilter(std::wstring const &keyword)
{
    m_keyword = keyword;
    m_vFiltered.clear();
    if (m_keyword.length() > 0)
    {
        for (size_t index = 0; index < m_vDisplay.size(); ++index)
        {
            bool match = false;
            for (size_t col = 0; !match && col < 9; ++col)
            {
                match = m_vDisplay[index][col].find(m_keyword) != std::wstring::npos;
            }
            if (match)
            {
                m_vFiltered.push_back(index);
            }
        }
    }
    if (m_pTableListView != nullptr)
    {
        HWND hListView = m_pTableListView->getHandle();
        if (m_vFiltered.empty())
        {
            ListView_SetItemCountEx(hListView, m_vDisplay.size(),
                LVSICF_NOINVALIDATEALL);
        }
        else
        {
            ListView_SetItemCountEx(hListView, m_vFiltered.size(),
                LVSICF_NOINVALIDATEALL);
        }
        if (!m_bLockVScroll)
        {
            m_pTableListView->scrollToBottom();
        }
    }
}

void MessageSession::parseMessage(json const &object)
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring timeZone = Bili::Settings::File::GetW("General", "timeZone");

    json value = {
        { "mID", m_nNextMessageID++ },
        { "room", object["room"] },
        { "time", object["time"] }
    };
    const json &data = object["data"];
    std::string protocol = data["cmd"];
    if (protocol == "WEBSOCKET")
    {
        value["protocol"] = "WebSocket";
        const json &info = data["msg"];

        if (info == "Connecting")
        {
            value["info"] = ResourceString(I18N::GetHandle(), IDS_WEBSOCKET_CONNECTING);
        }
        else if (info == "Established")
        {
            value["info"] = ResourceString(I18N::GetHandle(), IDS_WEBSOCKET_ESTABLISHED);
        }
        else if (info == "Closed")
        {
            value["info"] = ResourceString(I18N::GetHandle(), IDS_WEBSOCKET_CLOSED);
        }
        else if (info == "Failed")
        {
            value["info"] = ResourceString(I18N::GetHandle(), IDS_WEBSOCKET_FAILED);
        }
        else
        {
            value["info"] = ResourceString(I18N::GetHandle(), IDS_WEBSOCKET_UNKNOWN);
        }

        // value["info"] = data["msg"];
    }
    else if (protocol == "DANMU_MSG")
    {
        value["protocol"] = "Danmaku";
        value["info"] = data["info"];

        const json &info = data["info"];
        value["user"] = {
            { "id", info[2][0] },
            { "name", info[2][1] }
        };
        value["content"] = info[1];
    }
    else if (protocol == "SEND_GIFT")
    {
        value["protocol"] = "Gifting";
        value["info"] = data["data"];

        const json &info = data["data"];
        value["user"] = {
            { "id", info["uid"] },
            { "name", info["uname"] }
        };
        value["gift"] = {
            { "id", info["giftId"] },
            { "name", info["giftName"] },
            { "action", info["action"] },
            { "price", info["price"] }
        };
        value["count"] = info["num"];
    }
    else
    {
        value["protocol"] = "Unknown";
        value["info"] = data;
    }

    m_vMessage.push_back(value);

    std::array<std::wstring, 9> display;
    display[0] = std::to_wstring(value["room"].get<ROOM>());
    display[1] = std::to_wstring(value["mID"].get<long>());
    {
        std::time_t time = value["time"].get<std::time_t>();
        std::tm* t;
        if (timeZone == L"system")
        {
            t = std::localtime(&time);
        }
        else
        {
            int offset = 0;
            // Regex to match offset hours and minutes
            std::wregex regexUTC(L"\\(UTC(?:([+-]?[0-9]{1,2}?):([0-5][0-9]))\\)");
            std::wsmatch match;
            if (std::regex_search(timeZone, match, regexUTC))
            {
                int hour = std::stoi(match[1]);
                int minute = std::stoi(match[2]);
                offset = (hour * 60 + minute) * 60;
            }
            time += offset;
            t = std::gmtime(&time);
        }
        
        std::wstringstream wss;
        wss << std::put_time(t, L"%Y/%m/%d %H:%M:%S");
        display[2] = wss.str();
    }
    if (value["protocol"] == "Unknown")
    {
        display[3] = (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_PROTOCOL_UNKNOWN);
        display[7] = converter.from_bytes(value["info"].dump());
    }
    else if (value["protocol"] == "WebSocket")
    {
        display[3] = (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_PROTOCOL_PORT);
        display[7] = converter.from_bytes(value["info"].get<std::string>());
    }
    else if (value["protocol"] == "Danmaku")
    {
        display[3] = (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_PROTOCOL_DANMAKU);
        display[4] = std::to_wstring(value["user"]["id"].get<USER>());
        display[6] = converter.from_bytes(value["user"]["name"].get<std::string>());
        display[7] = converter.from_bytes(value["content"].get<std::string>());
    }
    else if (value["protocol"] == "Gifting")
    {
        display[3] = (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_PROTOCOL_GIFTING);
        display[4] = std::to_wstring(value["user"]["id"].get<USER>());
        display[6] = converter.from_bytes(value["user"]["name"].get<std::string>());
        {
            std::wstringstream wss;
            wss << converter.from_bytes(value["gift"]["action"].get<std::string>())
                << " "
                << converter.from_bytes(value["gift"]["name"].get<std::string>());
            if (value["count"] > 1)
            {
                wss << "x" << value["count"].get<long>();
            }
            display[7] = wss.str();
        }
    }
    m_vDisplay.push_back(display);

    if (m_keyword.length() > 0)
    {
        bool match = false;
        for (size_t col = 0; !match && col < 9; ++col)
        {
            match = display[col].find(m_keyword) != std::wstring::npos;
        }
        if (match)
        {
            m_vFiltered.push_back(m_vDisplay.size() - 1);
        }
    }

    if (m_pTableListView != nullptr)
    {
        HWND hListView = m_pTableListView->getHandle();
        if (m_vFiltered.empty())
        {
            ListView_SetItemCountEx(hListView, m_vDisplay.size(),
                LVSICF_NOINVALIDATEALL);
        }
        else
        {
            ListView_SetItemCountEx(hListView, m_vFiltered.size(),
                LVSICF_NOINVALIDATEALL);
        }
        if (!m_bLockVScroll)
        {
            m_pTableListView->scrollToBottom();
        }
    }
}