
#pragma once

#include "stdafx.h"
#include "controller.hpp"
#include "bili-https/bili-https.hpp"

Controller::Controller(HWND hWnd, CREATESTRUCT *pCreate) :
    m_hWnd(hWnd), m_model(L"Generic"), m_statusBar(hWnd, IDC_STATUSBAR),
    m_listView(hWnd, IDC_LISTVIEW), m_commandType(hWnd, IDC_COMMANDLINE_COMBO),
    m_commandLine(hWnd, IDC_COMMANDLINE_EDIT)
{
    // Initialize status bar.
    m_statusBar.setParts({ 400, 600, 800 });
    // Load these from string tables.
    m_statusBar.setText(0, L"Copyright (C) 2017 FrenzyLi");
    m_statusBar.setText(1, L"Cells");
    m_statusBar.setText(2, L"Text");
    m_statusBar.show();
    // Initialize list view control.
    m_listView.setColumns({
        (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_BILI_ROOM),
        (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_BILI_MSGID),
        (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_BILI_TIME),
        (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_BILI_PROTOCOL),
        (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_BILI_USERID),
        (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_BILI_USERTYPE),
        (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_BILI_USERNAME),
        (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_BILI_SUMMARY),
        (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_LIVEMAID_TRIGGER),
    }, { 80, 80, 150, 40, 80, 40, 180, 300, 40 });
    m_listView.show();
    // HWND header = m_listView.getHeader();
    // LONG_PTR styles = GetWindowLongPtr(header, GWL_STYLE);
    // SetWindowLongPtr(header, GWL_STYLE, styles | HDS_FILTERBAR | HDS_NOSIZING);
    // Initialize command line.
    m_commandType.show();
    m_commandType.add((LPCWSTR)
        ResourceString(I18N::GetHandle(), IDS_COMMANDTYPE_DANMAKU));
    m_commandType.add((LPCWSTR)
        ResourceString(I18N::GetHandle(), IDS_COMMANDTYPE_FILTERSTRING));
    m_commandType.add((LPCWSTR)
        ResourceString(I18N::GetHandle(), IDS_COMMANDTYPE_BANUSER));
    m_commandType.setSelection(0);
    m_commandLine.show();
    // Initialize chat room.
    m_session.setTableListView(m_listView);
    m_session.setStatusBar(m_statusBar);
}

Controller::~Controller()
{
    try
    {
        m_thread_fetch.join();
    }
    catch (const std::system_error &)
    {
        /* suppressed */
    }
    ::PostQuitMessage(0);
}

void Controller::command(int cmd)
{
    switch (cmd)
    {
    case IDM_EXIT:
        ::SendMessage(m_hWnd, WM_CLOSE, 0, 0L);
        break;

    case IDM_HELP:
        ::MessageBox(m_hWnd, L"Go figure!", L"Generic",
            MB_ICONINFORMATION | MB_OK);
        break;

    case IDM_ABOUT:
        DialogBox(I18N::GetHandle(),
            MAKEINTRESOURCE(IDD_ABOUTBOX), m_hWnd, AboutDlgProc);
        break;

    case IDM_CONNECTTO:
        if (!IsWindow(m_hWndConnect))
        {
            m_hWndConnect = (HWND)CreateDialog(I18N::GetHandle(),
                MAKEINTRESOURCE(IDD_CONNECT), m_hWnd, ConnectDlgProc);
        }
        ShowWindow(m_hWndConnect, SW_SHOW);
        SetActiveWindow(m_hWndConnect);
        break;

    case IDM_DISCONNECT:
        m_session.disconnect();
        break;

    case IDM_RECONNECT:
        m_session.reconnect();
        break;

    case IDM_DANMAKU_COPYSELECTED:
        m_listView.setClipboard();
        break;

    case IDM_DANMAKU_SELECTALL:
        m_listView.selectAll();
        break;

    case IDM_PREFERENCES:
        showPropertySheet();
        break;
    }
}

void Controller::connect()
{
    m_session.connect();
}

void Controller::disconnect()
{
    m_session.disconnect();
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(500ms);
}

LPCWSTR Controller::getActiveSessionColumn(int row, int col)
{
    return m_session.getColumn(row, col);
}

void Controller::initMenu(HMENU hMenu)
{
    int cMenuItems = GetMenuItemCount(hMenu);
    for (int nPos = 0; nPos < cMenuItems; nPos++)
    {
        UINT id = GetMenuItemID(hMenu, nPos);
        switch (id)
        {
        case IDM_CONNECTTO:
            if (m_session.isClosed())
            {
                EnableMenuItem(GetMenu(m_hWnd), id, MF_ENABLED);
            }
            else
            {
                EnableMenuItem(GetMenu(m_hWnd), id, MF_DISABLED | MF_GRAYED);
            }
            break;
        case IDM_RECONNECT:
        case IDM_DISCONNECT:
            if (m_session.isOpen())
            {
                EnableMenuItem(GetMenu(m_hWnd), id, MF_ENABLED);
            }
            else
            {
                EnableMenuItem(GetMenu(m_hWnd), id, MF_DISABLED | MF_GRAYED);
            }
            break;
        case IDM_DANMAKU_COPYSELECTED:
            int nSelected = m_listView.getSelectedCount();
            if (nSelected > 0)
            {
                EnableMenuItem(GetMenu(m_hWnd), id, MF_ENABLED);
            }
            break;
        }
    }
}

void Controller::notify(LPNMHDR lpNMHdr)
{
    switch (lpNMHdr->code)
    {
    case LVN_GETDISPINFO:
    {
        NMLVDISPINFO *pdi = (NMLVDISPINFO*)lpNMHdr;
        auto const &item = pdi->item;
        // The ID of the item to query.
        int itemid = item.iItem;
        // Does the list need string information?
        if (item.mask & LVIF_TEXT)
        {
            int iCol = item.iSubItem;
            std::wstring content = m_session.getColumn(itemid, iCol);
            // Copy the text to the LV_ITEM structure
            // Maximum number of characters is in pItem->cchTextMax
            lstrcpyn(item.pszText, content.c_str(), item.cchTextMax);
        }
    }
    }
}

void Controller::cycleMode(LPARAM lParam)
{
    int action = lParam;
    switch (action)
    {
    case -1:
        m_commandType.setSelectionNext();
        break;
    case 0:
        m_commandType.setSelection(0);
        break;
    case 1:
        m_commandType.setSelectionPrev();
        break;
    }
}

void Controller::paint()
{
    /*
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    // TODO: Add any drawing code that uses hdc here...
    EndPaint(hWnd, &ps);
    */
    PaintCanvas canvas(m_hWnd);
    m_view.Paint(canvas, m_model);
}

void Controller::setRoom(ROOM room)
{
    m_session.join(room);
    m_statusBar.setText(1, std::to_wstring(room));
}

void Controller::showPropertySheet()
{
    ResourceString title(I18N::GetHandle(), IDS_PREFERENCES);
    // Initialize property sheet pages.
    std::vector<HPROPSHEETPAGE> propSheetPages;
    PROPSHEETPAGE propSheetPage;
    propSheetPage.dwSize = sizeof(PROPSHEETPAGE);
    propSheetPage.dwFlags = PSP_DEFAULT;
    propSheetPage.hInstance = I18N::GetHandle();
    propSheetPage.pfnDlgProc = NULL;
    propSheetPage.lParam = (LPARAM)NULL;
    // ["I18N"]
    propSheetPage.pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_I18N);
    propSheetPage.pfnDlgProc = (DLGPROC)I18N_PropDlgProc;
    propSheetPages.push_back(CreatePropertySheetPage(&propSheetPage));
    // ["Session"]
    propSheetPage.pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_SESSION);
    propSheetPage.pfnDlgProc = (DLGPROC)Session_PropDlgProc;
    propSheetPages.push_back(CreatePropertySheetPage(&propSheetPage));
    // Initialize property sheet header.
    PROPSHEETHEADER propSheetHeader;
    propSheetHeader.dwSize = sizeof(PROPSHEETHEADER);
    propSheetHeader.dwFlags = PSH_MODELESS;
    propSheetHeader.hwndParent = m_hWnd;
    // propSheetHeader.hInstance = I18N::GetHandle();
    propSheetHeader.nPages = propSheetPages.size();
    propSheetHeader.phpage = propSheetPages.data();
    propSheetHeader.nStartPage = 0;
    propSheetHeader.pszCaption = (LPCWSTR)title;
    // Show property sheet.
    m_hPropSheet = (HWND)::PropertySheet(&propSheetHeader);
}

void Controller::size(int cx, int cy)
{
    m_view.SetSize(cx, cy);
    m_statusBar.setSize(cx);

    LONG heightStatusBar = m_statusBar.getHeight();
    LONG heightListView = cy - heightStatusBar;

    if (cx < 300)
    {
        m_commandType.hide();
        m_commandLine.hide();
    }
    else
    {
        LONG heightCommand = m_commandType.getHeight();
        heightListView -= heightCommand;

        m_commandType.setPosition(0, heightListView);
        m_commandType.setSize(80, 25);
        m_commandLine.setPosition(80, heightListView);
        m_commandLine.setSize(cx - 80, 25);
        m_commandType.show();
        m_commandLine.show();
    }

    // LONG height = m_statusBar.GetHeight();
    m_listView.setPosition(0, 0);
    m_listView.setSize(cx, heightListView);
}

void Controller::submit()
{
    std::wstring selection = m_commandType.getText();
    std::wstring content = m_commandLine.getContent();

    // Different action based on different command type.
    if (selection == (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_COMMANDTYPE_DANMAKU))
    {
        if (content.length() == 0)
        {
            return;
        }
        Bili::User::Credentials cred = Bili::Settings::GetCredentials();
        Bili::User::SendOptions options;
        options.roomid = m_session.getRoomID();
        options.msg = content;
        options.prepare();

        auto response = Bili::User::SendRoomChat(cred, options);
        if (response.is_array())
        {
            ResourceString success(I18N::GetHandle(), IDS_COMMANDLINE_DANMKAU_SUCCESS);
            m_statusBar.setText(0, (LPCWSTR)success);
            m_commandLine.clear();
        }
        else
        {
            std::wstringstream wss;
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
            ResourceString failure(I18N::GetHandle(), IDS_COMMANDLINE_DANMKAU_FAIL);
            wss << (LPCWSTR)failure << converter.from_bytes(response.dump());
            m_statusBar.setText(0, wss.str());
            ::MessageBeep(MB_ICONERROR);
        }
    }
    else if (selection == (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_COMMANDTYPE_BANUSER))
    {
    }
    else if (selection == (LPCWSTR)ResourceString(I18N::GetHandle(), IDS_COMMANDTYPE_FILTERSTRING))
    {
        m_session.setFilter(content);
    }
}


// Message handler for about box.
INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Message handler for about box.
INT_PTR CALLBACK ConnectDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_CONNECT_RESOLVE)
        {
            if (HIWORD(wParam) == EN_UPDATE)
            {
                WCHAR szContent[MAX_LOADSTRING];
                GetDlgItemText(hDlg, IDC_CONNECT_RESOLVE, szContent,
                    MAX_LOADSTRING);

                EnableWindow(GetDlgItem(hDlg, IDOK),
                    wcslen(szContent) > 0 ? TRUE : FALSE);

                return (INT_PTR)TRUE;
            }
        }
        else if (LOWORD(wParam) == IDOK)
        {
            WCHAR szContent[MAX_LOADSTRING];
            GetDlgItemText(hDlg, IDC_CONNECT_RESOLVE, szContent, MAX_LOADSTRING);
            std::wstring content = szContent;
            ROOM resolve = std::stoi(content);

            auto response = Bili::Room::GetResolve(resolve);

            // Ensure that the room being resolved is the current room.

            bool success = response.find("error") == response.end();
            if (success)
            {
                ROOM roomid = response["room_id"].get<ROOM>();
                SendMessage(GetParent(hDlg), WM_SET_ROOM, NULL, (LPARAM)roomid);
                SendMessage(GetParent(hDlg), WM_CONNECT, NULL, NULL);
                EndDialog(hDlg, LOWORD(wParam));
            }
            else
            {
                MessageBox(hDlg, L"LOL", L"NAY", MB_OK);
            }
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
