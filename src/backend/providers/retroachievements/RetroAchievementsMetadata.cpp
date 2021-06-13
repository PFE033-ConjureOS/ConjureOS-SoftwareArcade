// Pegasus Frontend
//
// Created by BozoTheGeek 07/06/2021
//

#include "RetroAchievementsMetadata.h"

#include "Log.h"
#include "model/gaming/Assets.h"
#include "model/gaming/Collection.h"
#include "model/gaming/Game.h"
#include "model/gaming/GameFile.h"
#include "providers/JsonCacheUtils.h"
#include "providers/SearchContext.h"
#include "utils/CommandTokenizer.h"
#include "utils/Zip.h"

#include "utils/rcheevos/include/rc_hash.h"

//#include "utils/MoveOnly.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QStringBuilder>
#include <QDir>

#include <QEventLoop>
#include <QElapsedTimer>
//#include <array>

#include <RecalboxConf.h>

namespace {
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

QString serialize_command(const QString& cmd, const QStringList& args)
{
    return (QStringList(QDir::toNativeSeparators(cmd)) + args).join(QLatin1String(" "));
}

QJsonDocument get_json_from_url(QString url_str, QString log_tag, QNetworkAccessManager &manager)
{
	QNetworkAccessManager* const manager_ptr = &manager;
	const QUrl url(url_str, QUrl::StrictMode);
	Q_ASSERT(url.isValid());
	if (Q_UNLIKELY(!url.isValid()))
	{
		Log::debug(log_tag, LOGMSG("Q_UNLIKELY(!url.isValid())"));
		return QJsonDocument();
	}
	
	//Set request
	QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
	#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
		request.setTransferTimeout(10000);
	#endif

	//Get request
    QNetworkReply* const reply = manager_ptr->get(request);
	
	//do loop on connect to wait donwload in this case
	QEventLoop loop;
	QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
	loop.exec();
	
	if (reply->error()) {
		Log::warning(log_tag, LOGMSG("Downloading metadata failed: %1").arg(reply->errorString()));
		return QJsonDocument();
	}
	const QByteArray raw_data = reply->readAll();
	QJsonDocument json = QJsonDocument::fromJson(raw_data);
	if (json.isNull()) {
		Log::warning(log_tag, LOGMSG(
			   "Failed to parse the response of the server, "
			   "either it's no longer available from https://retroachievements.org/ or the API has changed"));
		return QJsonDocument();
	}	
	return json;
}

QString apply_login_json(QString log_tag, const QJsonDocument& json)
//Example of JSON content
// {"Success":true,"User":"username","Token":"lePOt1iA5jr56cZj","Score":25,"Messages":0}
//from : http://retroachievements.org/dorequest.php?r=login&u=username&p=password
{
    using QL1 = QLatin1String;

    if (json.isNull())
	{
		Log::debug(log_tag, LOGMSG("json.isNull()"));
        return "";
	}
    const auto json_root = json.object();
    if (json_root.isEmpty())
	{
		Log::debug(log_tag, LOGMSG("json_root.isEmpty()")); 
		return "";
	}
    // const auto app_entry = json_root.begin().value().toObject();
    // if (app_entry.isEmpty())
        // return false;

    const bool login_success = json_root[QL1("Success")].toBool();
    if (!login_success)
	{
		Log::debug(log_tag, LOGMSG("login wrong")); 
		return "";
	}
	const QString user_data = json_root[QL1("User")].toString();
    if (user_data.isEmpty())
	{
		Log::debug(log_tag, LOGMSG("user_data.isEmpty()")); 
		return "";
	}
	else Log::debug(log_tag, LOGMSG("User: %1").arg(user_data));
	
	QString token_data = json_root[QL1("Token")].toString();
    if (token_data.isEmpty())
        return "";
	else 
	{
		Log::debug(log_tag, LOGMSG("Token: %1").arg(token_data));
	}
	
    return token_data;
}	

int apply_gameid_json(QString log_tag, const QJsonDocument& json)
//Example of JSON content
//  {"Success":true,"GameID":1669}
//or{"Success":true,"GameID":0} if wrong/game doesn't exist/recognized
//from : http://retroachievements.org/dorequest.php?r=gameid&m=44dca16afbee7fd947e86c30c1183846

{
    using QL1 = QLatin1String;

    if (json.isNull())
	{
		Log::debug(log_tag, LOGMSG("json.isNull()"));
        return 0;
	}
    const auto json_root = json.object();
    if (json_root.isEmpty())
	{
		Log::debug(log_tag, LOGMSG("json_root.isEmpty()")); 
		return 0;
	}

    const bool login_success = json_root[QL1("Success")].toBool();
    if (!login_success)
	{
		Log::debug(log_tag, LOGMSG("gameid request wrong")); 
		return 0;
	}
	const int gameid_data = json_root[QL1("GameID")].toInt();
	Log::debug(log_tag, LOGMSG("GameID: %1").arg(gameid_data));
	return gameid_data;
}	

QString get_token(QString log_tag, QString json_cache_dir, QNetworkAccessManager &manager)
//from : http://retroachievements.org/dorequest.php?r=login&u=username&p=password
{
	QElapsedTimer get_token_timer;
    get_token_timer.start();
	
	//GET information from recalbox.conf
	QString Username = QString::fromStdString(RecalboxConf::Instance().AsString("global.retroachievements.username"));
	QString Password = QString::fromStdString(RecalboxConf::Instance().AsString("global.retroachievements.password"));
	
	//Try to get token from json in cache
	QJsonDocument json = providers::read_json_from_cache(log_tag + " - cache", json_cache_dir, Username + Password);
	QString token = apply_login_json(log_tag + " - cache", json);
	if (token == "")
	{
		//Delete JSON inb cache by security - use Username and Password to have a unique key and if password is changed finally.
		providers::delete_cached_json(log_tag, json_cache_dir, Username + Password);

		//To get token
		const QString url_str = QStringLiteral("http://retroachievements.org/dorequest.php?r=login&u=%1&p=%2").arg(Username,Password);
		json = get_json_from_url(url_str, log_tag, manager);
		token = apply_login_json(log_tag, json);
		if (token != "")
		{
			//saved in cache
			providers::cache_json(log_tag, json_cache_dir, Username + Password, json.toJson(QJsonDocument::Compact));
		}
	}
	
	Log::info(log_tag, LOGMSG("Stats - Timing: Get token processing: %1ms").arg(get_token_timer.elapsed()));    
	return token;
}	

QString calculate_hash_from_file(QString rom_file, QString log_tag)
// can calculate hash using maner of RetroAchievements to manage the following system and format of rom supported by RetroAchievements.
{
	QElapsedTimer calculate_hash_timer;
    calculate_hash_timer.start();
	char hash_iterator[33] = "";
	int result_iterator;
	struct rc_hash_iterator iterator;
	const char* path = rom_file.toLocal8Bit().data();	
	rc_hash_initialize_iterator(&iterator, path, NULL, 0);
	result_iterator = rc_hash_iterate(hash_iterator, &iterator);
	rc_hash_destroy_iterator(&iterator);
	Log::info(log_tag, LOGMSG("Stats - Timing: Hash processing: %1ms").arg(calculate_hash_timer.elapsed()));    
	Log::debug(log_tag, LOGMSG("Hash on file: '%1' - '%2'").arg(rom_file, QString::fromLocal8Bit(hash_iterator)));
	return QString::fromLocal8Bit(hash_iterator);
}	

int get_gameid_from_hash(QString Hash, QString log_tag, QNetworkAccessManager &manager)
//from : http://retroachievements.org/dorequest.php?r=gameid&m=44dca16afbee7fd947e86c30c1183846
{
	QElapsedTimer get_gameid_timer;
    get_gameid_timer.start();
	int gameid = 0;
	
	if (Hash != "")
	{
		Log::debug(log_tag, LOGMSG("hash value to find GameID: '%1'").arg(Hash));
		//no cache usage in this cache, the cache will be manage by the game object itself by data stored inside.
		//To get GameID
		const QString url_str = QStringLiteral("http://retroachievements.org/dorequest.php?r=gameid&m=%1").arg(Hash);
		QJsonDocument json = get_json_from_url(url_str, log_tag, manager);
		gameid = apply_gameid_json(log_tag, json);
	}
	
	Log::info(log_tag, LOGMSG("Stats - Timing: Get GameID processing: %1ms").arg(get_gameid_timer.elapsed()));    
	return gameid;
}	
	
/* bool apply_json(model::Game& game, const QJsonDocument& json)
{
    using QL1 = QLatin1String;


    if (json.isNull())
        return false;

    const auto json_root = json.object();
    if (json_root.isEmpty())
        return false;

    const auto app_entry = json_root.begin().value().toObject();
    if (app_entry.isEmpty())
        return false;

    const bool app_success = app_entry[QL1("success")].toBool();
    if (!app_success)
        return false;

    const auto app_data = app_entry[QL1("data")].toObject();
    if (app_data.isEmpty())
        return false;

    // now the actual field reading

    model::Assets& assets = game.assetsMut(); // FIXME: update signals

    game.setTitle(app_data[QL1("name")].toString())
        .setSummary(app_data[QL1("short_description")].toString())
        .setDescription(app_data[QL1("about_the_game")].toString());

    const auto reldate_obj = app_data[QL1("release_date")].toObject();
    if (!reldate_obj.isEmpty()) {
        const QString date_str = reldate_obj[QL1("date")].toString();

        // FIXME: the date format will likely fail for non-English locales (see Qt docs)
        const QDateTime datetime(QDateTime::fromString(date_str, QL1("d MMM, yyyy")));
        if (datetime.isValid())
            game.setReleaseDate(datetime.date());
    }

    const QString header_image = app_data[QL1("header_image")].toString();
    assets
        .add_uri(AssetType::LOGO, header_image)
        .add_uri(AssetType::UI_STEAMGRID, header_image)
        .add_uri(AssetType::BOX_FRONT, header_image);

    const QJsonArray developer_arr = app_data[QL1("developers")].toArray();
    for (const auto& arr_entry : developer_arr)
        game.developerList().append(arr_entry.toString());

    const QJsonArray publisher_arr = app_data[QL1("publishers")].toArray();
    for (const auto& arr_entry : publisher_arr)
        game.publisherList().append(arr_entry.toString());

    const auto metacritic_obj = app_data[QL1("metacritic")].toObject();
    if (!metacritic_obj.isEmpty()) {
        const double score = metacritic_obj[QL1("score")].toDouble(-1);
        if (0.0 <= score && score <= 100.0)
            game.setRating(static_cast<float>(score / 100.0));
    }

    const auto genre_arr = app_data[QL1("genres")].toArray();
    for (const auto& arr_entry : genre_arr) {
        const auto genre_obj = arr_entry.toObject();
        if (genre_obj.isEmpty())
            break; // assume the rest will fail too

        const QString genre = genre_obj[QL1("description")].toString();
        if (!genre.isEmpty())
            game.genreList().append(genre);
    }

    const auto category_arr = app_data[QL1("categories")].toArray();
    for (const auto& arr_entry : category_arr) {
        const auto cat_obj = arr_entry.toObject();
        if (cat_obj.isEmpty())
            break; // assume the rest will fail too

        const QString category = cat_obj[QL1("description")].toString();
        if (!category.isEmpty())
            game.tagList().append(category);
    }

    const QString background_image = app_data[QL1("background")].toString();
    if (!background_image.isEmpty())
        assets.add_uri(AssetType::BACKGROUND, background_image);

    const auto screenshots_arr = app_data[QL1("screenshots")].toArray();
    for (const auto& arr_entry : screenshots_arr) {
        const auto screenshot_obj = arr_entry.toObject();
        if (screenshot_obj.isEmpty())
            break; // assume the rest will fail too

        const QString thumb_path = screenshot_obj[QL1("path_thumbnail")].toString();
        if (!thumb_path.isEmpty())
            assets.add_uri(AssetType::SCREENSHOT, thumb_path);
    }

    const auto movies_arr = app_data[QL1("movies")].toArray();
    for (const auto& arr_entry : movies_arr) {
        const auto movie_obj = arr_entry.toObject();
        if (movie_obj.isEmpty())
            break;

        const auto webm_obj = movie_obj[QL1("webm")].toObject();
        if (webm_obj.isEmpty())
            break;

        const QString p480_path = webm_obj[QL1("480")].toString();
        if (!p480_path.isEmpty())
            assets.add_uri(AssetType::VIDEO, p480_path);
    }

    return true;
} */
} // namespace


namespace providers {
namespace retroAchievements {

Metadata::Metadata(QString log_tag)
    : m_log_tag(std::move(log_tag))
    , m_json_cache_dir(QStringLiteral("retroachievements"))
{
}

void Metadata::fill_from_network(model::Game& game) const
{
	QString token;

	//Set Game info
	model::Game* const game_ptr = &game;
    QString title = game_ptr->title();
    
	//check if recalbox.conf to know if activated
	if (!RecalboxConf::Instance().AsBool("global.retroachievements"))
	{
		Log::debug(m_log_tag, LOGMSG("not activated !"));
        return;
	}

	//Create Network Access 
	QNetworkAccessManager *manager = new QNetworkAccessManager(game_ptr->parent());
	
	//GetToken first from cache or network
	token = get_token(m_log_tag, m_json_cache_dir, *manager);
	if (token != "")
	{
		//check if gameid exists and hash already calculated
		if(game_ptr->RaGameID() == 0)
		{
			Log::debug(m_log_tag, LOGMSG("RetroAchievement RaGameId to find !"));
			const model::GameFile* gamefile = game_ptr->filesConst().first(); /// take into account only the first file for the moment.
			const QFileInfo& finfo = gamefile->fileinfo();		
			//QString romfile = PathMakeEscaped(QDir::toNativeSeparators(finfo.absoluteFilePath()));
			QString romfile = QDir::toNativeSeparators(finfo.absoluteFilePath());
			QString targetfile;
			//check if zip
			if(romfile.toLower().endsWith(".zip"))
			{	
				Zip zip(Path(romfile.toLocal8Bit().data()));
				Log::debug(m_log_tag, LOGMSG("This zip has %1 file(s).").arg(zip.Count()));
				if(zip.Count() == 1)
				{
					//it seems a console game because only one file is present
					//unzip file_unzipped
					//example : unzip -o -d /tmp "/recalbox/share/roms/nes/Duck Hunt (World).zip"
					QString UnzipCommand = "unzip";
					QStringList args = QStringList {
											QStringLiteral("-o"),
											QStringLiteral("-d /tmp"),
											"\""+romfile+"\""
										};
					int exitcode = system(qPrintable(serialize_command(UnzipCommand, args)));

					//set target file from /tmp
					targetfile = "/tmp/" + QString::fromStdString(zip.FileName(0).ToString());
				}	
				else	
				{
					//could be a arcade game in this case
					//set target file from as the initial zip due to the fact that hash for arcade use the name of the file
					targetfile = romfile;					
				}
			}
			else
			{	//not zipped
				//set target file as the intial romfile
				targetfile = romfile;	
			}
			Log::debug(m_log_tag, LOGMSG("The target file to hash is '%1'").arg(targetfile));
			QString hash = calculate_hash_from_file(targetfile, m_log_tag);
			
			//check if tmp file used
			if(targetfile.toLower().startsWith("/tmp/"))
			{
				Log::debug(m_log_tag, LOGMSG("Deletion of target file : '%1'").arg(targetfile));
				//delete file
				QString DeleteFileCommand = "rm";
				QStringList args = QStringList {"\""+targetfile+"\""};
				int exitcode = system(qPrintable(serialize_command(DeleteFileCommand, args)));
			}
			
			//FOR TEST ONLY
			//QString file_unzipped = "/recalbox/share/roms/nes/Duck Hunt (World).nes";
			//Log::debug(m_log_tag, LOGMSG("Rom Filepath to hash: '%1'").arg(file_unzipped));
			//const char* path = file_unzipped.toLocal8Bit().data();		


/* 			//methods without console_id to provide (better)
			char hash_iterator[33] = "";
			int result_iterator;
			struct rc_hash_iterator iterator;
			
			//if zip, if one file only, unzip file in buffer and calculate hash
			QString file_zipped = finfo.absoluteFilePath();
			Zip zip(Path(file_zipped.toLocal8Bit().data()));
			Log::debug(m_log_tag, LOGMSG("This zip has %1 file(s).").arg(zip.Count()));
			if(zip.Count() == 1)
			{
				Log::debug(m_log_tag, LOGMSG("The file in zip is named : %1").arg(zip.FileName(0));
				Log::debug(m_log_tag, LOGMSG("The file in zip has as size of %1 octet(s).").arg(zip.UncompressedSize(0)));
				const char* tmp_path = file_unzipped.toLocal8Bit().data();	
				rc_hash_initialize_iterator(&iterator, tmp_path, NULL, 0);
				result_iterator = rc_hash_iterate(hash_iterator, &iterator);
				rc_hash_destroy_iterator(&iterator);
				Log::debug(m_log_tag, LOGMSG("Hash of file (using iterator and file unzipped): '%1'").arg(QString::fromLocal8Bit(hash_iterator)));
			}

			//methogs with console_id to provide (for complex)
			char hash_file[33] = "";
			int console_id = RC_CONSOLE_NINTENDO;
			const char* path2 = file_unzipped.toLocal8Bit().data();
			int result_file = rc_hash_generate_from_file(hash_file, console_id, path2);
			Log::debug(m_log_tag, LOGMSG("Hash of file (using console_id): '%1'").arg(QString::fromLocal8Bit(hash_file)));

			//for test: methods with zip extension as for arcade games (neogeo, fbneo, mame, atomiswave, naomi, naomigd, etc...)	
			char hash_iterator3[33] = "";
			int result_iterator3;
			struct rc_hash_iterator iterator3;
			const char* path3 = file_zipped.toLocal8Bit().data();
			rc_hash_initialize_iterator(&iterator3, path3, NULL, 0);
			result_iterator3 = rc_hash_iterate(hash_iterator3, &iterator3);
			rc_hash_destroy_iterator(&iterator3);
			Log::debug(m_log_tag, LOGMSG("Hash of file (using iterator and zip format as for arcade games): '%1'").arg(QString::fromLocal8Bit(hash_iterator3)));
 */
			
			//TEST HASH of Nes - Duck Hunt
			//QString hash = "fa382374eb4a93a719064ca6c5a4e78c";
			//Log::debug(m_log_tag, LOGMSG("Duck Hunt (World) - NES - hash value from RA : '%1'").arg(hash));
			
			game_ptr->setRaGameID(get_gameid_from_hash(hash,m_log_tag, *manager));
			Log::debug(m_log_tag, LOGMSG("RetroAchievement GameId found is : %1").arg(game_ptr->RaGameID()));
			

		}
		else
		{
			Log::debug(m_log_tag, LOGMSG("RetroAchievement GameId already known : %1").arg(game_ptr->RaGameID()));
		}
		
		//for test purpose / we reset gameid to be able to retest just after ;-)
		game_ptr->setRaGameID(0);
		
	}
	else return;
	
/* 	const QUrl url(url_str, QUrl::StrictMode);
	Q_ASSERT(url.isValid());
	if (Q_UNLIKELY(!url.isValid()))
	{
		Log::debug(log_tag, LOGMSG("Q_UNLIKELY(!url.isValid())"));
		return;
	}
	
	
	
	
	//Set request
	QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
	#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
		request.setTransferTimeout(10000);
	#endif

	//Get request
    QNetworkReply* const reply = manager->get(request); */

	//Manage reply

//FOR TEST PURPOSE ONLY
/* 	QObject::connect(reply, &QNetworkReply::finished, [=]() {
		if(reply->error() == QNetworkReply::NoError)
		{
			QByteArray response = reply->readAll();
			// do something with the data...
			Log::debug(LOGMSG("response = %1").arg(QString::fromStdString(response.toStdString())));
		}
		else // handle error
		{
	      Log::debug(LOGMSG("ERROR"));
		}
		
	}); */	
	
	//sctx.schedule_download(url, [title, game_ptr, log_tag, json_cache_dir](QNetworkReply* const reply){
	// QObject::connect(reply, &QNetworkReply::finished, [=]() {	
        // if (reply->error()) {
            // Log::warning(log_tag, LOGMSG("Downloading metadata for `%1` failed: %2")
                // .arg(title, reply->errorString()));
            // return;
        // }
		// Log::debug(log_tag, LOGMSG("1 - sctx.schedule_download(url, [log_tag, json_cache_dir, game_ptr, title](QNetworkReply* const reply)"));
        // const QByteArray raw_data = reply->readAll();
        // const QJsonDocument json = QJsonDocument::fromJson(raw_data);
        // if (json.isNull()) {
            // Log::warning(log_tag, LOGMSG(
                   // "Failed to parse the response of the server for game '%1', "
                   // "either it's no longer available from the Steam Store or the Steam API has changed"
               // ).arg(title));
            // return;
        // }

		
    // });
}

} // namespace retroAchievements
} // namespace providers
