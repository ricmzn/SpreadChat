#include "SpreadConnection.h"
#include "SpreadWorker.h"
#include <sp.h>
#include <sstream>
#include <algorithm>

std::string SpreadConnection::getVersion()
{
    std::stringstream ss;
    int major, minor, patch;
    SP_version(&major, &minor, &patch);
    ss << major << "." << minor << "." << patch;
    return ss.str();
}

SpreadConnection::SpreadConnection()
    : connected(false)
{}

SpreadConnection::SpreadConnection(const char* user, const char* host, int port)
{
    connect(user, host, port);
}

SpreadConnection::~SpreadConnection()
{
    disconnect();
}

bool SpreadConnection::connect(const char* user, const char* host, int port)
{
    disconnect();
    std::stringstream ss;
    ss << port << "@" << host;
    char group[MAX_GROUP_NAME];
    int status = SP_connect(ss.str().c_str(), user, 0, 1, &mailbox, group);
    connected = (status == ACCEPT_SESSION);
    if (!connected) {
        lastError = -status;
        return false;
    }
    ss.str("");
    ss << host << ":" << port;
    hostname = ss.str();
    worker.start();
    return true;
}

void SpreadConnection::disconnect()
{
    if (connected) {
        QMetaObject::invokeMethod(&worker, "finish");
        worker.wait();
        SP_disconnect(mailbox);
        connected = false;
        hostname = {};
    }
}

bool SpreadConnection::isConnected() const
{
    return connected;
}

std::string SpreadConnection::getHostname() const
{
    return hostname;
}

int SpreadConnection::getLastError() const
{
    return lastError;
}

const SpreadConnection::SpreadGroupList& SpreadConnection::getGroups() const
{
    return groups;
}

const SpreadGroup* SpreadConnection::joinGroup(const char* group)
{
    int status = SP_join(mailbox, group);
    if (status != 0) {
        lastError = status;
        return nullptr;
    }
    groups.push_back(std::make_unique<SpreadGroup>(this, group));
    return groups.back().get();
}

bool SpreadConnection::inGroup(const char* name) const
{
    auto result = std::find_if(groups.begin(), groups.end(), [&](const auto& val) {
        return val->getName() == name;
    });
    return result != groups.end();
}

void SpreadConnection::leaveGroup(const SpreadGroup* group)
{
    SP_leave(mailbox, group->getName().c_str());
    auto iter = std::remove_if(groups.begin(), groups.end(), [&](const auto& val) {
        volatile bool is_equal = val->getName() == group->getName();
        return val->getName() == group->getName();
    });
    groups.erase(iter, groups.end());
}
