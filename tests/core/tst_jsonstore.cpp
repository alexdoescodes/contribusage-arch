#include "core/apppaths.h"
#include "core/jsonstore.h"

#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

using namespace contribusage;

namespace {

constexpr int version = 2;

QJsonObject sample(const QString &label) {
    return {{QStringLiteral("label"), label}};
}

QByteArray contents(const QString &file) {
    QFile input(file);
    return input.open(QIODevice::ReadOnly) ? input.readAll() : QByteArray();
}

} // namespace

// SPEC §10.7 and §16.3: atomic writes, schema versions, history never discarded, provider data deletion.
class TestJsonStore : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void roundTripCreatesTheRootOwnerOnly() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.filePath(QStringLiteral("contribusage"))};
        const QString file = paths.providerFolder(QStringLiteral("fake")) + QStringLiteral("/history.json");

        const auto missing = jsonstore::read(file, version);
        QVERIFY(missing && !missing->has_value());

        QVERIFY(jsonstore::write(file, version, sample(QStringLiteral("a"))));

        const auto read = jsonstore::read(file, version);
        QVERIFY(read && read->has_value());
        QCOMPARE(read->value().toObject(), sample(QStringLiteral("a")));
        QCOMPARE(contents(file), QByteArray(R"({"schemaVersion":2,"value":{"label":"a"}})"));
        const auto permissions = QFile::permissions(paths.root);
        QVERIFY(permissions.testFlags(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(!(permissions & (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther |
                                 QFile::WriteOther | QFile::ExeOther)));
    }

    // An atomic write replaces the file instead of rewriting it: a hard link to the old file keeps the old content.
    void writeReplacesTheFileAtomically() {
        QTemporaryDir temporary;
        const QString file = temporary.filePath(QStringLiteral("state.json"));
        const QString link = temporary.filePath(QStringLiteral("link.json"));
        QVERIFY(jsonstore::write(file, version, sample(QStringLiteral("old"))));
        QCOMPARE(::link(QFile::encodeName(file).constData(), QFile::encodeName(link).constData()), 0);

        QVERIFY(jsonstore::write(file, version, sample(QStringLiteral("new"))));

        QCOMPARE(jsonstore::read(file, version)->value().toObject(), sample(QStringLiteral("new")));
        QCOMPARE(jsonstore::read(link, version)->value().toObject(), sample(QStringLiteral("old")));
    }

    // Another version is an error and the file stays as it is: caches overwrite it, history.json keeps it.
    void versionMismatchKeepsTheFile() {
        QTemporaryDir temporary;
        const QString file = temporary.filePath(QStringLiteral("history.json"));
        const QByteArray newer = R"({"schemaVersion":3,"value":{"label":"future"}})";
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write(newer);
        output.close();

        const auto read = jsonstore::read(file, version);
        QVERIFY(!read);
        QCOMPARE(read.error().kind, StoreError::Kind::UnsupportedSchemaVersion);
        QCOMPARE(read.error().version, 3);
        QCOMPARE(contents(file), newer);
    }

    void malformedFileIsAnError() {
        QTemporaryDir temporary;
        const QString file = temporary.filePath(QStringLiteral("state.json"));
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        output.write("{ not json");
        output.close();

        const auto read = jsonstore::read(file, version);
        QVERIFY(!read);
        QCOMPARE(read.error().kind, StoreError::Kind::Malformed);
        QCOMPARE(contents(file), QByteArray("{ not json"));
    }

    void deletingProviderDataRemovesOnlyThatProvidersFolder() {
        QTemporaryDir temporary;
        const AppPaths paths{.root = temporary.filePath(QStringLiteral("contribusage"))};
        const QString history = QStringLiteral("/history.json");
        QVERIFY(jsonstore::write(paths.providerFolder(QStringLiteral("fake")) + history, version, sample({})));
        QVERIFY(jsonstore::write(paths.providerFolder(QStringLiteral("other")) + history, version, sample({})));
        QVERIFY(jsonstore::write(paths.root + QStringLiteral("/state.json"), version, sample({})));

        QVERIFY(jsonstore::deleteProviderData(QStringLiteral("fake"), paths));
        QVERIFY(jsonstore::deleteProviderData(QStringLiteral("fake"), paths)); // already gone is not an error

        QVERIFY(!QFile::exists(paths.providerFolder(QStringLiteral("fake"))));
        QVERIFY(QFile::exists(paths.providerFolder(QStringLiteral("other"))));
        QVERIFY(QFile::exists(paths.root + QStringLiteral("/state.json")));
    }

    // Port design §2: the data folder follows XDG.
    void liveRootFollowsXdgDataHome() {
        QTemporaryDir temporary;
        const QByteArray saved = qgetenv("XDG_DATA_HOME");
        qputenv("XDG_DATA_HOME", QFile::encodeName(temporary.path()));
        const QString root = AppPaths::live().root;
        if (saved.isEmpty()) {
            qunsetenv("XDG_DATA_HOME");
        } else {
            qputenv("XDG_DATA_HOME", saved);
        }
        QCOMPARE(root, temporary.path() + QStringLiteral("/contribusage"));
        QCOMPARE(AppPaths{.root = root}.providerFolder(QStringLiteral("claude-code")),
                 root + QStringLiteral("/providers/claude-code"));
    }
};

QTEST_GUILESS_MAIN(TestJsonStore)
#include "tst_jsonstore.moc"
