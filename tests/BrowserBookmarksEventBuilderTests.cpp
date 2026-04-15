#include <QtTest>

#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "connectors/BrowserBookmarksEventBuilder.h"

class BrowserBookmarksEventBuilderTests : public QObject
{
    Q_OBJECT

private slots:
    void buildsBookmarkEventFromChromiumFile();
    void rejectsInvalidBookmarksFile();
};

void BrowserBookmarksEventBuilderTests::buildsBookmarkEventFromChromiumFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/Bookmarks");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << R"({
        "roots": {
            "bookmark_bar": {
                "children": [
                    {
                        "type": "url",
                        "name": "OpenAI release updates",
                        "url": "https://openai.com/news",
                        "date_added": "13253760000000000"
                    }
                ]
            }
        }
    })";
    file.close();

    const ConnectorEvent event = BrowserBookmarksEventBuilder::fromBookmarksFile(
        path,
        QStringLiteral("Edge"),
        QFileInfo(path).lastModified().toUTC());
    QVERIFY(event.isValid());
    QCOMPARE(event.sourceKind, QStringLiteral("connector_research_browser"));
    QCOMPARE(event.connectorKind, QStringLiteral("research"));
    QCOMPARE(event.taskType, QStringLiteral("web_search"));
    QCOMPARE(event.summary, QStringLiteral("Research updated: OpenAI release updates"));
}

void BrowserBookmarksEventBuilderTests::rejectsInvalidBookmarksFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.path() + QStringLiteral("/Bookmarks");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&file);
    stream << "{}";
    file.close();

    const ConnectorEvent event = BrowserBookmarksEventBuilder::fromBookmarksFile(
        path,
        QStringLiteral("Chrome"),
        QFileInfo(path).lastModified().toUTC());
    QVERIFY(!event.isValid());
}

QTEST_APPLESS_MAIN(BrowserBookmarksEventBuilderTests)
#include "BrowserBookmarksEventBuilderTests.moc"
