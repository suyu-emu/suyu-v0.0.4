#ifndef MIGRATION_WORKER_H
#define MIGRATION_WORKER_H

#include <QObject>

class MigrationWorker : public QObject
{
    Q_OBJECT
public:
    enum class LegacyEmu {
        Citron,
        Sudachi,
        Yuzu,
        Suyu,
    };

    enum class MigrationStrategy {
        Copy,
        Move,
        Link,
    };

    MigrationWorker(const LegacyEmu selected_legacy_emu,
                    const bool clear_shader_cache,
                    const MigrationStrategy strategy);

public slots:
    void process();

signals:
    void finished(const QString &success_text);
    void error(const QString &error_message);

private:
    LegacyEmu selected_legacy_emu;
    bool clear_shader_cache;
    MigrationStrategy strategy;
    QString success_text = tr("Data was migrated successfully.");
};

#endif // MIGRATION_WORKER_H
