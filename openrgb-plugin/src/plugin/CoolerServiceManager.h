#pragma once

#include <memory>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include "Ax120Settings.h"

class CoolerServiceManager : public QObject
{
    Q_OBJECT

public:
    explicit CoolerServiceManager(QObject* parent = nullptr);

    bool        IsBusy() const;
    bool        IsInstalled() const;
    bool        NeedsUpdate() const;
    QString     Activity() const;
    QString     Message() const;

    void        Install(const Ax120PluginSettings& settings);
    void        Remove();
    void        Refresh();

signals:
    void        Changed();

private:
    std::unique_ptr<QTemporaryDir>  staging;
    QProcess*                       operation = nullptr;
    QProcess*                       activityQuery = nullptr;
    QString                         activity;
    QString                         message;
    bool                            installed   = false;
    bool                            needsUpdate = false;

    QString                         StageProgram();
    void                            Launch(const QStringList& programArguments, const QString& successMessage);
    void                            Finish(int exitCode, const QString& successMessage);
    static QByteArray               EmbeddedProgram();
    static QString                  CurrentUser();
};
