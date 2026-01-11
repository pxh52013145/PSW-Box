#include "passwordcsv.h"

#include <QUrl>

namespace {

QChar detectDelimiter(const QString &line)
{
    const int commas = line.count(',');
    const int semicolons = line.count(';');
    const int tabs = line.count('\t');

    if (tabs >= commas && tabs >= semicolons && tabs > 0)
        return '\t';
    if (semicolons > commas)
        return ';';
    return ',';
}

struct CsvTable final
{
    QStringList header;
    QVector<QStringList> rows;
};

CsvTable parseCsvText(const QString &text, QChar delimiter, QString *errorOut)
{
    CsvTable table;

    QStringList row;
    QString field;
    bool inQuotes = false;

    const auto flushField = [&]() {
        row.push_back(field);
        field.clear();
    };

    const auto flushRow = [&]() {
        flushField();
        table.rows.push_back(row);
        row.clear();
    };

    for (int i = 0; i < text.size(); ++i) {
        const auto ch = text.at(i);
        if (inQuotes) {
            if (ch == '"') {
                const bool hasNext = (i + 1) < text.size();
                if (hasNext && text.at(i + 1) == '"') {
                    field.append('"');
                    i++;
                } else {
                    inQuotes = false;
                }
            } else {
                field.append(ch);
            }
            continue;
        }

        if (ch == '"') {
            inQuotes = true;
            continue;
        }

        if (ch == delimiter) {
            flushField();
            continue;
        }

        if (ch == '\n') {
            flushRow();
            continue;
        }

        if (ch == '\r') {
            continue;
        }

        field.append(ch);
    }

    if (inQuotes) {
        if (errorOut)
            *errorOut = "CSV 解析失败：引号不匹配";
        return table;
    }

    if (!field.isEmpty() || !row.isEmpty())
        flushRow();

    if (table.rows.isEmpty())
        return table;

    table.header = table.rows.takeFirst();
    return table;
}

int headerIndex(const QStringList &header, const QStringList &candidates)
{
    for (int i = 0; i < header.size(); ++i) {
        const auto h = header.at(i).trimmed().toLower();
        for (const auto &candidate : candidates) {
            if (h == candidate)
                return i;
        }
    }
    return -1;
}

QString valueAt(const QStringList &row, int index)
{
    if (index < 0 || index >= row.size())
        return {};
    return row.at(index).trimmed();
}

QString deriveTitle(const QString &urlText)
{
    const auto url = QUrl(urlText.trimmed());
    if (url.isValid() && !url.host().trimmed().isEmpty())
        return url.host();
    if (!urlText.trimmed().isEmpty())
        return urlText.trimmed();
    return "未命名";
}

QString csvEscape(const QString &value)
{
    auto out = value;
    const bool needQuote = out.contains(',') || out.contains('"') || out.contains('\n') || out.contains('\r');
    out.replace("\"", "\"\"");
    if (needQuote)
        out = QString("\"%1\"").arg(out);
    return out;
}

QString joinTags(const QStringList &tags)
{
    QStringList out;
    out.reserve(tags.size());
    for (const auto &t : tags) {
        const auto trimmed = t.trimmed();
        if (!trimmed.isEmpty())
            out.push_back(trimmed);
    }
    return out.join(",");
}

} // namespace

PasswordCsvInfo detectPasswordCsv(const QByteArray &csvData, QString *errorOut)
{
    PasswordCsvInfo info;

    auto text = QString::fromUtf8(csvData);
    if (!text.isEmpty() && text.at(0) == QChar(0xFEFF))
        text.remove(0, 1);

    const auto firstLine = text.section('\n', 0, 0);
    info.delimiter = detectDelimiter(firstLine);

    QString error;
    const auto table = parseCsvText(text, info.delimiter, &error);
    if (!error.isEmpty()) {
        if (errorOut)
            *errorOut = error;
        return info;
    }

    info.header = table.header;
    if (info.header.isEmpty()) {
        if (errorOut)
            *errorOut = "CSV 为空或缺少表头";
        return info;
    }

    const auto hasPassword = headerIndex(info.header, {"password", "pass"}) >= 0;
    const auto hasUsername = headerIndex(info.header, {"username", "user", "login", "login_username"}) >= 0;
    const auto hasUrl = headerIndex(info.header, {"url", "website", "origin", "formactionorigin"}) >= 0;
    const auto hasTitle = headerIndex(info.header, {"title"}) >= 0;
    const auto hasName = headerIndex(info.header, {"name"}) >= 0;
    const auto hasGroup = headerIndex(info.header, {"group"}) >= 0;
    info.hasCategoryLikeColumn = headerIndex(info.header, {"category", "folder", "group"}) >= 0;

    if (hasGroup && hasTitle && hasUsername && hasPassword && hasUrl) {
        info.format = PasswordCsvFormat::KeePassXC;
    } else if (hasName && hasUsername && hasPassword && hasUrl) {
        info.format = PasswordCsvFormat::Chrome;
    } else if (hasTitle && hasPassword) {
        info.format = PasswordCsvFormat::Toolbox;
    } else {
        info.format = PasswordCsvFormat::Unknown;
    }

    return info;
}

PasswordCsvParseResult parsePasswordCsv(const QByteArray &csvData, qint64 defaultGroupId)
{
    PasswordCsvParseResult result;
    if (defaultGroupId <= 0)
        defaultGroupId = 1;

    auto text = QString::fromUtf8(csvData);
    if (!text.isEmpty() && text.at(0) == QChar(0xFEFF))
        text.remove(0, 1);

    const auto firstLine = text.section('\n', 0, 0);
    const auto delimiter = detectDelimiter(firstLine);

    QString error;
    auto table = parseCsvText(text, delimiter, &error);
    if (!error.isEmpty()) {
        result.error = error;
        return result;
    }

    if (table.header.isEmpty()) {
        result.error = "CSV 为空或缺少表头";
        return result;
    }

    const auto passwordIdx = headerIndex(table.header, {"password", "pass"});
    const auto usernameIdx = headerIndex(table.header, {"username", "user", "login", "login_username"});
    const auto urlIdx = headerIndex(table.header, {"url", "website", "origin", "formactionorigin"});
    const auto titleIdx = headerIndex(table.header, {"title", "name"});
    const auto notesIdx = headerIndex(table.header, {"notes", "note", "comment"});
    const auto categoryIdx = headerIndex(table.header, {"category", "folder", "group"});
    const auto tagsIdx = headerIndex(table.header, {"tags", "tag"});

    if (passwordIdx < 0) {
        result.error = "未找到密码列（需要表头包含 password）";
        return result;
    }

    result.totalRows = table.rows.size();

    for (const auto &row : table.rows) {
        PasswordEntrySecrets secrets;
        secrets.entry.groupId = defaultGroupId;

        secrets.entry.title = titleIdx >= 0 ? valueAt(row, titleIdx) : QString();
        secrets.entry.username = usernameIdx >= 0 ? valueAt(row, usernameIdx) : QString();
        secrets.password = valueAt(row, passwordIdx);
        secrets.entry.url = urlIdx >= 0 ? valueAt(row, urlIdx) : QString();
        secrets.entry.category = categoryIdx >= 0 ? valueAt(row, categoryIdx) : QString();
        secrets.notes = notesIdx >= 0 ? valueAt(row, notesIdx) : QString();

        if (tagsIdx >= 0) {
            auto tagsText = valueAt(row, tagsIdx);
            tagsText.replace("，", ",");
            tagsText.replace("；", ",");
            tagsText.replace(";", ",");
            for (const auto &tag : tagsText.split(',', Qt::SkipEmptyParts)) {
                const auto t = tag.trimmed();
                if (!t.isEmpty())
                    secrets.entry.tags.push_back(t);
            }
        }

        if (secrets.entry.title.isEmpty())
            secrets.entry.title = deriveTitle(secrets.entry.url);

        if (secrets.password.isEmpty()) {
            result.skippedEmpty++;
            continue;
        }

        if (secrets.entry.title.trimmed().isEmpty()) {
            result.skippedInvalid++;
            continue;
        }

        result.entries.push_back(secrets);
    }

    if (titleIdx < 0)
        result.warnings.push_back("CSV 缺少标题列（已从 URL 自动生成）");
    if (urlIdx < 0)
        result.warnings.push_back("CSV 缺少 URL 列（将无法做站点匹配）");

    return result;
}

QByteArray exportPasswordCsv(const QVector<PasswordEntrySecrets> &entries)
{
    QString out;
    out.reserve(entries.size() * 128);

    out.append("title,username,password,url,category,tags,notes\r\n");
    for (const auto &e : entries) {
        out.append(csvEscape(e.entry.title));
        out.append(',');
        out.append(csvEscape(e.entry.username));
        out.append(',');
        out.append(csvEscape(e.password));
        out.append(',');
        out.append(csvEscape(e.entry.url));
        out.append(',');
        out.append(csvEscape(e.entry.category));
        out.append(',');
        out.append(csvEscape(joinTags(e.entry.tags)));
        out.append(',');
        out.append(csvEscape(e.notes));
        out.append("\r\n");
    }

    static const QByteArray bom("\xEF\xBB\xBF");
    return bom + out.toUtf8();
}
