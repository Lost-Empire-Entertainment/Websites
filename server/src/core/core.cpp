//Copyright(C) 2026 Lost Empire Entertainment
//This program comes with ABSOLUTELY NO WARRANTY.
//This is free software, and you are welcome to redistribute it under certain conditions.
//Read LICENSE.md for more information.

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <signal.h>
#endif

#include <filesystem>
#include <thread>
#include <chrono>

#include "KalaHeaders/log_utils.hpp"
#include "KalaHeaders/thread_utils.hpp"

#include "server/ks_server.hpp"
#include "server/ks_cloudflare.hpp"
#include "server/ks_connect.hpp"
#include "server/ks_response.hpp"

#include "core/core.hpp"

using KalaHeaders::KalaLog::Log;
using KalaHeaders::KalaLog::LogType;

using KalaHeaders::KalaThread::abool;

using KalaServer::Server::ServerCore;
using KalaServer::Server::Cloudflare;
using KalaServer::Server::Connect;
using KalaServer::Server::Connection;
using KalaServer::Server::Response;
using KalaServer::Server::ResponseData;
using KalaServer::Server::ResponseType;
using KalaServer::Server::ContentType;

#ifdef _WIN32
using std::wstring;
#endif

using std::filesystem::path;
using std::filesystem::current_path;
using std::filesystem::weakly_canonical;
using std::this_thread::sleep_for;
using std::chrono::milliseconds;
using std::string;

static abool running(true);

#ifdef _WIN32
BOOL WINAPI HandleClose(DWORD signal)
{
    if (signal == CTRL_C_EVENT
        || signal == CTRL_CLOSE_EVENT)
    {
        running.store(false);
    }
    
    return TRUE;
}
#else
static void HandleClose(int) { running.store(false); }
#endif

namespace WebsiteBackend::Core
{
    void WebsiteBackendCore::Initialize()
    {
#ifdef _WIN32
        SetConsoleCtrlHandler(HandleClose, TRUE);
#else
        signal(SIGINT, HandleClose);
        signal(SIGTERM, HandleClose);
#endif

        Log::Print(
            "Initializing website backend",
            "WEBSITE_BACKEND",
             LogType::LOG_INFO);

        Connect::AddRoute("/");

        Connect::AddBlacklistedKeyword("/wp-");
        Connect::AddBlacklistedKeyword("/user");
        Connect::AddBlacklistedKeyword("/login");
        Connect::AddBlacklistedKeyword("/admin");
        Connect::AddBlacklistedKeyword(".php");
        Connect::AddBlacklistedKeyword(".env");
        Connect::AddBlacklistedKeyword(".git");
        Connect::AddBlacklistedKeyword(".json");
        Connect::AddBlacklistedKeyword(".sql");
        Connect::AddBlacklistedKeyword(".sh");
        Connect::AddBlacklistedKeyword("bin");

        path contentPath = weakly_canonical(current_path() / ".." / ".." / "content");

        if (!exists(contentPath))
        {
            Log::Print(
                "Failed to find content path '" + contentPath.string() + "'!",
                "WEBSITE_BACKEND",
                LogType::LOG_ERROR,
                2);

            exit(1);
        }

        Log::Print(
            "Found content path '" + contentPath.string() + "'",
            "WEBSITE_BACKEND",
            LogType::LOG_INFO);

        if (!ServerCore::HasInternet())
        {
            Log::Print(
                "Server '" + string(ServerCore::GetServerName()) + "' has no internet, cannot initialize!",
                "WEBSITE_BACKEND",
                LogType::LOG_ERROR,
                2);

            exit(1);
        }

        if (!ServerCore::Initialize(
             "website_backend",
            contentPath,
            { "thekalakit.com", "elypsoengine.com" },
            "213.101.197.212",
            80,
            false))
        {
            exit(1);
        }

        if (ServerCore::IsCloudflareRequired())
        {
#ifdef _WIN32
            PWSTR winUserDir{};
            SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &winUserDir);

            path userDir = path(winUserDir) / ".cloudflared";
            CoTaskMemFree(winUserDir);

            path cloudflareExePath = current_path() / "cloudflared-windows-amd64.exe";
#else
            path userDir = path(getenv("HOME")) / ".cloudflared";
            path cloudflareExePath = current_path() / "cloudflared-linux-amd64";
#endif

            if (!Cloudflare::Initialize(
                "website_backend_tunnel", 
                cloudflareExePath, 
                userDir))
            {
                exit(1);
            }

            while (!ServerCore::IsReady())
            {
                sleep_for(milliseconds(1000));

                Log::Print(
                    "Waiting for Cloudflare to finish connecting...",
                    "WEBSITE_BACKEND",
                    LogType::LOG_INFO);
            }
        }

        Log::Print(
            "Website backend is ready, starting accept loop!",
             "WEBSITE_BACKEND",
             LogType::LOG_INFO);

        if (!Connect::CreateListenerSocket()) exit(1);

        //mandatory loop to keep server alive
        while (running.load()) { sleep_for(milliseconds(10)); }

        Log::Print(
            "Website backend is shutting down.",
            "WEBSITE_BACKEND",
            LogType::LOG_INFO);

        ServerCore::Shutdown();
    }
}