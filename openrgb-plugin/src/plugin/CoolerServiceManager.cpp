#include "CoolerServiceManager.h"

#include <pwd.h>
#include <unistd.h>
#include <QFile>
#include "CoolerServiceFiles.h"

namespace
{
    const QString   EmbeddedProgramResource = ":/service/diy-rgb-tools-cooler";
    const QString   PkexecPath              = "/usr/bin/pkexec";
    const QString   SystemctlPath           = "/usr/bin/systemctl";
    constexpr int   PkexecNotAuthorized     = 126;
    constexpr int   PkexecAuthenticationFailed = 127;
}

CoolerServiceManager::CoolerServiceManager(QObject* parent)
    : QObject(parent)
{
    Refresh();
}

bool CoolerServiceManager::IsBusy() const
{
    return operation != nullptr;
}

bool CoolerServiceManager::IsInstalled() const
{
    return installed;
}

bool CoolerServiceManager::NeedsUpdate() const
{
    return needsUpdate;
}

QString CoolerServiceManager::Activity() const
{
    return activity;
}

QString CoolerServiceManager::Message() const
{
    return message;
}

void CoolerServiceManager::Install(const Ax120PluginSettings& settings)
{
    const QString programPath = StageProgram();

    if(programPath.isEmpty())
    {
        return;
    }

    const QString settingsPath = staging->filePath("cooler.json");

    try
    {
        CoolerServiceFiles::WriteSettings(settingsPath.toStdString(), settings);
    }
    catch(const std::exception& error)
    {
        message = QString("Could not prepare the settings: %1").arg(error.what());
        emit Changed();
        return;
    }

    Launch({ programPath, "install", "--owner", CurrentUser(), "--settings", settingsPath },
           "Installed. The display now runs from early boot whenever OpenRGB is closed.");
}

void CoolerServiceManager::Remove()
{
    const QString programPath = StageProgram();

    if(!programPath.isEmpty())
    {
        Launch({ programPath, "remove" }, "Removed. The display stays dark while OpenRGB is closed.");
    }
}

void CoolerServiceManager::Refresh()
{
    installed   = CoolerServiceFiles::IsInstalled();
    needsUpdate = false;

    if(installed)
    {
        QFile installedProgram(CoolerServiceFiles::ProgramPath);
        needsUpdate = !installedProgram.open(QIODevice::ReadOnly) || installedProgram.readAll() != EmbeddedProgram();
    }

    if(activityQuery != nullptr)
    {
        return;
    }

    activityQuery = new QProcess(this);
    connect(activityQuery, &QProcess::finished, this, [this]
    {
        activity = QString::fromUtf8(activityQuery->readAllStandardOutput()).trimmed();
        activityQuery->deleteLater();
        activityQuery = nullptr;
        emit Changed();
    });
    activityQuery->start(SystemctlPath, { "is-active", CoolerServiceFiles::ServiceName });
}

QString CoolerServiceManager::StageProgram()
{
    staging = std::make_unique<QTemporaryDir>();
    const QByteArray program = EmbeddedProgram();

    if(!staging->isValid() || program.isEmpty())
    {
        message = "Could not unpack the service program from the plugin.";
        emit Changed();
        return QString();
    }

    const QString programPath = staging->filePath("diy-rgb-tools-cooler");
    QFile         programFile(programPath);

    if(!programFile.open(QIODevice::WriteOnly) || programFile.write(program) != program.size())
    {
        message = "Could not unpack the service program from the plugin.";
        emit Changed();
        return QString();
    }

    programFile.close();
    programFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return programPath;
}

void CoolerServiceManager::Launch(const QStringList& programArguments, const QString& successMessage)
{
    if(operation != nullptr)
    {
        return;
    }

    operation = new QProcess(this);
    operation->setProcessChannelMode(QProcess::MergedChannels);
    connect(operation, &QProcess::finished, this, [this, successMessage](int exitCode, QProcess::ExitStatus exitStatus)
    {
        Finish(exitStatus == QProcess::NormalExit ? exitCode : -1, successMessage);
    });
    connect(operation, &QProcess::errorOccurred, this, [this, successMessage](QProcess::ProcessError error)
    {
        if(error == QProcess::FailedToStart)
        {
            Finish(-1, successMessage);
        }
    });

    message = geteuid() == 0 ? "Working…" : "Waiting for your password…";

    if(geteuid() == 0)
    {
        operation->start(programArguments.front(), programArguments.mid(1));
    }
    else
    {
        operation->start(PkexecPath, programArguments);
    }

    emit Changed();
}

void CoolerServiceManager::Finish(int exitCode, const QString& successMessage)
{
    const QString output = QString::fromUtf8(operation->readAll()).trimmed();
    operation->deleteLater();
    operation = nullptr;
    staging.reset();

    if(exitCode == 0)
    {
        message = successMessage;
    }
    else if(exitCode == PkexecNotAuthorized || exitCode == PkexecAuthenticationFailed)
    {
        message = "Nothing changed: the password prompt was cancelled or refused.";
    }
    else
    {
        message = "Failed: " + (output.isEmpty() ? QString("pkexec could not run the service program") : output);
    }

    Refresh();
    emit Changed();
}

QByteArray CoolerServiceManager::EmbeddedProgram()
{
    static const QByteArray program = []
    {
        QFile resource(EmbeddedProgramResource);
        return resource.open(QIODevice::ReadOnly) ? resource.readAll() : QByteArray();
    }();
    return program;
}

QString CoolerServiceManager::CurrentUser()
{
    const passwd* user = getpwuid(getuid());
    return user != nullptr ? QString::fromLocal8Bit(user->pw_name) : QString::number(getuid());
}
