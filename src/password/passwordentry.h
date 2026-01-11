#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

enum class PasswordEntryType : int
{
    WebLogin = 0,
    DesktopClient = 1,
    ApiKeyToken = 2,
    DatabaseCredential = 3,
    ServerSsh = 4,
    DeviceWifi = 5,
};

inline PasswordEntryType passwordEntryTypeFromInt(int value)
{
    switch (value) {
    case static_cast<int>(PasswordEntryType::WebLogin):
        return PasswordEntryType::WebLogin;
    case static_cast<int>(PasswordEntryType::DesktopClient):
        return PasswordEntryType::DesktopClient;
    case static_cast<int>(PasswordEntryType::ApiKeyToken):
        return PasswordEntryType::ApiKeyToken;
    case static_cast<int>(PasswordEntryType::DatabaseCredential):
        return PasswordEntryType::DatabaseCredential;
    case static_cast<int>(PasswordEntryType::ServerSsh):
        return PasswordEntryType::ServerSsh;
    case static_cast<int>(PasswordEntryType::DeviceWifi):
        return PasswordEntryType::DeviceWifi;
    default:
        return PasswordEntryType::WebLogin;
    }
}

inline QString passwordEntryTypeLabel(PasswordEntryType type)
{
    switch (type) {
    case PasswordEntryType::WebLogin:
        return "Web 登录";
    case PasswordEntryType::DesktopClient:
        return "桌面/客户端";
    case PasswordEntryType::ApiKeyToken:
        return "API Key/Token";
    case PasswordEntryType::DatabaseCredential:
        return "数据库凭据";
    case PasswordEntryType::ServerSsh:
        return "服务器/SSH";
    case PasswordEntryType::DeviceWifi:
        return "设备/Wi-Fi";
    default:
        return "Web 登录";
    }
}

struct PasswordEntry final
{
    qint64 id = 0;
    qint64 groupId = 1;
    PasswordEntryType type = PasswordEntryType::WebLogin;
    QString title;
    QString username;
    QString url;
    QString category;
    QStringList tags;
    QDateTime createdAt;
    QDateTime updatedAt;
};

struct PasswordEntrySecrets final
{
    PasswordEntry entry;
    QString password;
    QString notes;
};
