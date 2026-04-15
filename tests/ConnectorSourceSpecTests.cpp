#include <QtTest>

#include "connectors/ConnectorSourceSpec.h"

class ConnectorSourceSpecTests : public QObject
{
    Q_OBJECT

private slots:
    void returnsExpectedDefaultSources();
};

void ConnectorSourceSpecTests::returnsExpectedDefaultSources()
{
    const QList<ConnectorSourceSpec> specs = defaultConnectorSourceSpecs();
    QCOMPARE(specs.size(), 0);
}

QTEST_APPLESS_MAIN(ConnectorSourceSpecTests)
#include "ConnectorSourceSpecTests.moc"
