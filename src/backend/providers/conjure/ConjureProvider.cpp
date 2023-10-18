//
// Created by jonny on 2023-10-17.
// Started from duplication of ConjureProvider.cpp
//

#include "ConjureProvider.h"

#include "Log.h"
#include "Paths.h"
#include "providers/ProviderUtils.h"
#include "providers/SearchContext.h"
#include "providers/conjure/ConjureMetadata.h"
#include "providers/conjure/ConjureFilter.h"
#include "utils/StdHelpers.h"
#include "utils/PathTools.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QStringBuilder>
#include <QDirIterator>

namespace {
    bool is_conjure_metadata_file(const QString &filename) {
        // TODO:  maybe enforce metadata.conjure.txt ?
        return filename == QLatin1String("metadata.pegasus.txt")
               || filename == QLatin1String("metadata.conjure.txt")
               || filename == QLatin1String("metadata.txt")
               || filename.endsWith(QLatin1String(".metadata.pegasus.txt"))
               || filename.endsWith(QLatin1String(".metadata.conjure.txt"))
               || filename.endsWith(QLatin1String(".metadata.txt"));
    }

    std::vector<QString> find_metafiles_in(const QString &dir_path) {
        constexpr auto dir_filters = QDir::Files | QDir::NoDotAndDotDot;
        constexpr auto dir_flags = QDirIterator::FollowSymlinks;

        std::vector<QString> result;

        QDirIterator dir_it(dir_path, dir_filters, dir_flags);
        while (dir_it.hasNext()) {
            dir_it.next();
            if (is_conjure_metadata_file(dir_it.fileName())) {
                QString path = ::clean_abs_path(dir_it.fileInfo());
                result.emplace_back(std::move(path));
            }
        }

        return result;
    }

} // namespace

namespace providers {
    namespace conjure {
        ConjureProvider::ConjureProvider(QObject *parent)
                : Provider(QLatin1String("conjure_metafiles"), QStringLiteral("Conjure Metafiles"),
                           PROVIDER_FLAG_INTERNAL, parent) {}

        Provider &ConjureProvider::run(SearchContext &sctx) {

            //TODO move this to Path.cpp
            const QString conjure_root =
                    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "\\ConjureGames";

            std::vector<QString> metafile_paths = find_metafiles_in(conjure_root);
            if (metafile_paths.empty()) {
                Log::info(display_name(), LOGMSG("No metadata files found"));
                return *this;
            }
            const Metadata metahelper(display_name());
            std::vector<FileFilter> all_filters;

            const float progress_step = 1.f / metafile_paths.size();
            float progress = 0.f;

            for (const QString &path: metafile_paths) {
                Log::info(display_name(), LOGMSG("Found `%1`").arg(::pretty_path(path)));

                std::vector<FileFilter> filters = metahelper.apply_metafile(path, sctx);
                all_filters.insert(all_filters.end(),
                                   std::make_move_iterator(filters.begin()),
                                   std::make_move_iterator(filters.end()));

                progress += progress_step;
                emit progressChanged(progress);
            }

            for (FileFilter &filter: all_filters) {
                apply_filter(filter, sctx);

                for (QString &dir_path: filter.directories)
                    sctx.pegasus_add_game_dir(dir_path);
            }

            return *this;
        }
    } // namespace conjure
} // namespace providers