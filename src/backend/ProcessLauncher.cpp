// Pegasus Frontend
// Copyright (C) 2017-2019  Mátyás Mustoha
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.


#include "ProcessLauncher.h"
#include "ScriptManager.h"

#include "Log.h"
#include "ScriptRunner.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "platform/TerminalKbd.h"
#include "utils/CommandTokenizer.h"
#include "utils/Strings.h"
#include "utils/PathTools.h"

//to access es_input.cfg
#include "providers/es2/Es2Provider.h"

//For recalbox
#include "RecalboxConf.h"


#include <QDir>
#include <QRegularExpression>

namespace {
static constexpr auto SEPARATOR = "----------------------------------------";

void replace_env_vars(QString& param)
{
    const auto env = QProcessEnvironment::systemEnvironment();
    const QRegularExpression rx_env(QStringLiteral("{env.([^}]+)}"));

    auto match = rx_env.match(param);
    while (match.hasMatch()) {
        const int from = match.capturedStart();
        const int len = match.capturedLength();
        param.replace(from, len, env.value(match.captured(1)));

        const int match_offset = match.capturedStart() + match.capturedLength();
        match = rx_env.match(param, match_offset);
    }
}

QString PathMakeEscaped(QString param)
{
  std::string escaped = param.toUtf8().constData();

  static std::string invalidChars = " '\"\\!$^&*(){}[]?;<>";
  const char* invalids = invalidChars.c_str();
  for(int i = escaped.size(); --i >= 0; )
  {
    char c = escaped.c_str()[i];
    for(int j = invalidChars.size(); --j >= 0; )
      if (c == invalids[j])
      {
        escaped.insert(i, "\\");
        break;
      }
  }

  return QString::fromStdString(escaped);
}

void replace_variables(QString& param, const model::GameFile* q_gamefile)
{
    Q_ASSERT(q_gamefile);
    
    const model::GameFile& gamefile = *q_gamefile;
    const model::Game& game = *gamefile.parentGame();
    const QFileInfo& finfo = gamefile.fileinfo();
    
    Log::debug(LOGMSG("Param not updated: '%1'").arg(param));
    
    param.replace(QLatin1String("{file.path}"), PathMakeEscaped(QDir::toNativeSeparators(finfo.absoluteFilePath())));
    
    QString shortname = game.systemShortName();
    
    param
        .replace(QLatin1String("{file.name}"), finfo.fileName())
        .replace(QLatin1String("{file.basename}"), finfo.completeBaseName())
        .replace(QLatin1String("{file.dir}"), QDir::toNativeSeparators(finfo.absolutePath()))
        .replace(QLatin1String("{system.shortname}"),shortname)
        .replace(QLatin1String("{emulator.ratio}"),QString::fromStdString(RecalboxConf::Instance().AsString("global.ratio")))
        .replace(QLatin1String("{emulator.netplay}"),"");

    if(param == "{emulator.name}")
    {   
        QString emulator = QString::fromStdString(RecalboxConf::Instance().AsString(shortname.append(".emulator").toUtf8().constData()));
        if(emulator != "")
        {
            param.replace(QLatin1String("{emulator.name}"),emulator);
        }
        else  param.replace(QLatin1String("{emulator.name}"),game.emulatorName());
        
    }            
    
    if(param == "{emulator.core}")
    {
        QString core = QString::fromStdString(RecalboxConf::Instance().AsString(shortname.append(".core").toUtf8().constData()));
        if(core != "")
        {
            param.replace(QLatin1String("{emulator.core}"),core);
        }
        else  param.replace(QLatin1String("{emulator.core}"),game.emulatorCore());
    }            
    
    if(param == "{controllers.config}")
    {
        // Fill from ES/Recalbox configuration methods
        std::string uuid, name, path;
        std::string command = "";
        
        int MaxInputDevices = 10;
        //to access ES provider
        providers::es2::Es2Provider *Provider = new providers::es2::Es2Provider();
                      
        for(int player = 0; player < MaxInputDevices; ++player)
        {
            path = "";
            uuid = "";
            name = "";

            if (Strings::SplitInThree(RecalboxConf::Instance().GetPadPegasus(player), '|', uuid, name, path, true))
            {
              //example of example : -p1index 0 -p1guid 030000005e040000a102000000010000 -p1name \"Xbox 360 Wireless Receiver\" -p1nbaxes 4 -p1nbhats 1 -p1nbbuttons 17 -p1devicepath /dev/input/event3
              //                       -p1index 0 -p1guid 030000005e040000a102000000010000 -p1name "X360 Wireless Controller" -p1nbaxes 4 -p1nbhats 1 -p1nbbuttons 17 -p1devicepath /dev/input/event19 -p2index 1 -p2guid 030000005e040000a102000000010000 -p2name "X360 Wireless Controller" -p2nbaxes 4 -p2nbhats 1 -p2nbbuttons 17 -p2devicepath /dev/input/event20 -system 64dd -rom /recalbox/share/roms/64dd/Super\ Mario\ 64\ -\ Disk\ Version\ \(Japan\)\ \(Proto\).ndd -emulator libretro -core parallel_n64 -ratio custom 
              //const InputDevice& device = mIdToDevices[pad.Identifier];

              const providers::es2::inputConfigEntry inputConfigEntry = Provider->load_input_data(QString::fromStdString(name), QString::fromStdString(uuid));
              
              if (inputConfigEntry.inputConfigAttributs.deviceName == QString::fromStdString(name))
              {//Device found 
                  std::string p(" -p"); p.append(Strings::ToString(player + 1)); 
                  command.append(p).append("index ").append(Strings::ToString(player)) //TO DO: see how to manage order between connection order/player order.
                         .append(p).append("guid ").append(uuid)
                         .append(p).append("name \"").append(name + "\"")
                         .append(p).append("nbaxes ").append(inputConfigEntry.inputConfigAttributs.deviceNbAxes.toUtf8().constData())
                         .append(p).append("nbhats ").append(inputConfigEntry.inputConfigAttributs.deviceNbHats.toUtf8().constData())
                         .append(p).append("nbbuttons ").append(inputConfigEntry.inputConfigAttributs.deviceNbButtons.toUtf8().constData())
                         .append(p).append("devicepath ").append(path);
              }
            }

        }
        param.replace(QLatin1String("{controllers.config}"),QString::fromStdString(command));
    }
    
    replace_env_vars(param);
    
    Log::debug(LOGMSG("Param updated: '%1'").arg(param));

}

bool contains_slash(const QString& str)
{
    return str.contains(QChar('/')) || str.contains(QChar('\\'));
}

QString serialize_command(const QString& cmd, const QStringList& args)
{
    return (QStringList(QDir::toNativeSeparators(cmd)) + args).join(QLatin1String(" "));
}

QString processerror_to_string(QProcess::ProcessError error)
{
    switch (error) {
        case QProcess::FailedToStart:
            return LOGMSG("Could not launch `%1`. Either the program is missing, "
                          "or you don't have the permission to run it.");
        case QProcess::Crashed:
            return LOGMSG("The external program `%1` has crashed");
        case QProcess::Timedout:
            return LOGMSG("The command `%1` did not start in a reasonable amount of time");
        case QProcess::ReadError:
        case QProcess::WriteError:
            // We don't communicate with the launched process at the moment
            Q_UNREACHABLE();
            break;
        default:
            return LOGMSG("Running the command `%1` failed due to an unknown error");
    }
}
} // namespace


namespace helpers {
QString abs_launchcmd(const QString& cmd, const QString& base_dir)
{
    Q_ASSERT(!cmd.isEmpty());

    if (!contains_slash(cmd))
        return cmd;

    return ::clean_abs_path(QFileInfo(base_dir, cmd));
}

QString abs_workdir(const QString& workdir, const QString& base_dir, const QString& fallback_workdir)
{
    if (workdir.isEmpty())
        return fallback_workdir;

    return ::clean_abs_path(QFileInfo(base_dir, workdir));
}
} // namespace helpers


ProcessLauncher::ProcessLauncher(QObject* parent)
    : QObject(parent)
    , m_process(nullptr)
{}

void ProcessLauncher::onLaunchRequested(const model::GameFile* q_gamefile)
{
    Q_ASSERT(q_gamefile);
    
    const model::GameFile& gamefile = *q_gamefile;
    const model::Game& game = *gamefile.parentGame();

    QString raw_launch_cmd =
#if defined(Q_OS_LINUX) && defined(PEGASUS_INSIDE_FLATPAK)
        QLatin1String("flatpak-spawn --host ") % game.launchCmd();
#else
        game.launchCmd();
#endif

    QStringList args = ::utils::tokenize_command(raw_launch_cmd);
    
    for (QString& arg : args)
        replace_variables(arg, &gamefile);

    QString command = args.isEmpty() ? QString() : args.takeFirst();
    if (command.isEmpty()) {
        const QString message = LOGMSG("Cannot launch the game `%1` because there is no launch command defined for it.")
            .arg(game.title());
        Log::warning(message);
        emit processLaunchError(message);
        return;
    }
    command = helpers::abs_launchcmd(command, game.launchCmdBasedir());

#if defined(Q_OS_WINDOWS)
    const QFileInfo command_finfo(command);
    if (command_finfo.isShortcut()) {
        args = QStringList {
            QStringLiteral("/q"),
            QStringLiteral("/c"),
            command,
        } + args;
        command = QStringLiteral("cmd");
    }
#endif

    const QString default_workdir = contains_slash(command)
        ? QFileInfo(command).absolutePath()
        : gamefile.fileinfo().absolutePath();

    QString workdir = game.launchWorkdir();
    replace_variables(workdir, &gamefile);
    
    workdir = helpers::abs_workdir(workdir, game.launchCmdBasedir(), default_workdir);

    //legacy script system (from pegasus)
    beforeRun(gamefile.fileinfo().absoluteFilePath());
    
    //notify game launching for es_states.tmp (from recalbox)
    ScriptManager::Instance().Notify(&gamefile, Notification::RunGame);
    runProcess(command, args, workdir);
}

void ProcessLauncher::runProcess(const QString& command, const QStringList& args, const QString& workdir)
{
    Log::info(LOGMSG("Executing command: [`%1`]").arg(serialize_command(command, args)));
    Log::info(LOGMSG("Working directory: `%3`").arg(QDir::toNativeSeparators(workdir)));

    Q_ASSERT(!m_process);
    m_process = new QProcess(this);

    // set up signals and slots
    connect(m_process, &QProcess::started, this, &ProcessLauncher::onProcessStarted);
    connect(m_process, &QProcess::errorOccurred, this, &ProcessLauncher::onProcessError);
    connect(m_process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this, &ProcessLauncher::onProcessFinished);

    // run the command
    m_process->setProcessChannelMode(QProcess::ForwardedChannels);
    
    m_process->setInputChannelMode(QProcess::ForwardedInputChannel);
    
    m_process->setWorkingDirectory(workdir);
    
    //init of global Command
    globalCommand = "";
        
    // 2 ways to launch: in case of Python to launch retroarch, we can't use only Qprocess
    if (command.contains("python"))
    {
        emit processLaunchOk(); // to stop front-end
        // put command and args in global variables to launch when Front-end Tear Down is Completed !
        globalCommand = command;
        globalArgs = args;
    }
    else
    {
        m_process->start(command, args, QProcess::ReadOnly);
        m_process->waitForStarted(-1);
    }
  
}

void ProcessLauncher::onTeardownComplete()
{
    Q_ASSERT(m_process);

    if(globalCommand.length() != 0)
    {
        int exitcode = system(qPrintable(serialize_command(globalCommand, globalArgs)));
        if (exitcode == 0) ProcessLauncher::onProcessFinished(exitcode, QProcess::NormalExit);
        else ProcessLauncher::onProcessFinished(exitcode, QProcess::CrashExit);
        
    }
    else
    {
        m_process->waitForFinished(-1);
    }
    emit processFinished();
}

void ProcessLauncher::onProcessStarted()
{
    Q_ASSERT(m_process);

    Log::debug(LOGMSG("Program: %1").arg(m_process->program()));
    Log::info(LOGMSG("Process %1 started").arg(m_process->processId()));
    Log::info(SEPARATOR);

    emit processLaunchOk();
}

void ProcessLauncher::onProcessError(QProcess::ProcessError error)
{
    Q_ASSERT(m_process);

    const QString message = processerror_to_string(error).arg(m_process->program());

    switch (m_process->state()) {
        case QProcess::Starting:
        case QProcess::NotRunning:
            emit processLaunchError(message);
            Log::warning(message);
            afterRun(); // finished() won't run
            break;

        case QProcess::Running:
            emit processRuntimeError(message);
            break;
    }
}

void ProcessLauncher::onProcessFinished(int exitcode, QProcess::ExitStatus exitstatus)
{
    Q_ASSERT(m_process);
    
    Log::info(SEPARATOR);

    switch (exitstatus) {
        case QProcess::NormalExit:
            if (exitcode == 0)
                Log::info(LOGMSG("The external program has finished cleanly"));
            else
                Log::warning(LOGMSG("The external program has finished with error code %1").arg(exitcode));
            break;
        case QProcess::CrashExit:
            Log::warning(LOGMSG("The external program has crashed"));
            break;
    }
    
    //notify game finishing for es_states.tmp (from recalbox)
    ScriptManager::Instance().Notify(Notification::EndGame);

    afterRun();
}

void ProcessLauncher::beforeRun(const QString& game_path)
{
    TerminalKbd::enable();
    ScriptRunner::run(ScriptEvent::PROCESS_STARTED, { game_path });
}

void ProcessLauncher::afterRun()
{
    Q_ASSERT(m_process);
    
    m_process->deleteLater();
    m_process = nullptr;
    
    ScriptRunner::run(ScriptEvent::PROCESS_FINISHED);
    TerminalKbd::disable();
}
