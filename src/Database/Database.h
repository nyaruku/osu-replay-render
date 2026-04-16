#pragma once
#include <string>
#include <optional>
#include <filesystem>
#include <stdexcept>
#include <unordered_map>
#include <sqlite3.h>

namespace Orr::Database {

    struct BeatmapEntry {
        std::string file_path;
        std::string md5;
        std::string title;
        std::string artist;
        std::string creator;
        std::string version;
        int beatmap_id = 0;
        int beatmap_set_id = 0;
    };

    struct BeatmapDb {
        sqlite3* db = nullptr;

        BeatmapDb(const BeatmapDb&) = delete;
        BeatmapDb& operator=(const BeatmapDb&) = delete;

        BeatmapDb(const std::string& dbPath);
        ~BeatmapDb();
    };

    std::optional<BeatmapEntry> FindByMd5(BeatmapDb& bdb, const std::string& md5);

    namespace helper {

        inline std::string ColText(sqlite3_stmt* stmt, int col) {
            const unsigned char* text = sqlite3_column_text(stmt, col);
            return text ? reinterpret_cast<const char*>(text) : "";
        }

        inline void InitSchema(sqlite3* db) {
            const char* sql =
                "CREATE TABLE IF NOT EXISTS beatmaps ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "file_path TEXT NOT NULL UNIQUE,"
                "md5 TEXT NOT NULL,"
                "last_modified INTEGER NOT NULL DEFAULT 0,"
                "title TEXT DEFAULT '',"
                "artist TEXT DEFAULT '',"
                "creator TEXT DEFAULT '',"
                "version TEXT DEFAULT '',"
                "beatmap_id INTEGER DEFAULT 0,"
                "beatmap_set_id INTEGER DEFAULT 0"
                ");"
                "CREATE INDEX IF NOT EXISTS idx_md5 ON beatmaps(md5);";
            char* err = nullptr;
            sqlite3_exec(db, sql, nullptr, nullptr, &err);
            if (err) {
                std::string msg = err;
                sqlite3_free(err);
                throw std::runtime_error("BeatmapDb schema error: " + msg);
            }
        }

        inline bool IsUpToDate(sqlite3* db, const std::filesystem::path& path, int64_t last_mod) {
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db, "SELECT last_modified FROM beatmaps WHERE file_path = ?;", -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, path.string().c_str(), -1, SQLITE_TRANSIENT);
            bool result = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                result = sqlite3_column_int64(stmt, 0) == last_mod;
            }
            sqlite3_finalize(stmt);
            return result;
        }

        inline std::unordered_map<std::string, int64_t> LoadAllModTimes(sqlite3* db) {
            std::unordered_map<std::string, int64_t> result;
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db, "SELECT file_path, last_modified FROM beatmaps;", -1, &stmt, nullptr);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                result[ColText(stmt, 0)] = sqlite3_column_int64(stmt, 1);
            }
            sqlite3_finalize(stmt);
            return result;
        }

        inline void Upsert(sqlite3* db, const BeatmapEntry& entry, int64_t last_mod) {
            sqlite3_stmt* stmt;
            sqlite3_prepare_v2(db,
                "INSERT OR REPLACE INTO beatmaps "
                "(file_path, md5, last_modified, title, artist, creator, version, beatmap_id, beatmap_set_id) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
                -1, &stmt, nullptr);
            sqlite3_bind_text(stmt, 1, entry.file_path.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, entry.md5.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 3, last_mod);
            sqlite3_bind_text(stmt, 4, entry.title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, entry.artist.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, entry.creator.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 7, entry.version.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 8, entry.beatmap_id);
            sqlite3_bind_int(stmt, 9, entry.beatmap_set_id);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }

    }

    inline BeatmapDb::BeatmapDb(const std::string& dbPath) {
        if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("BeatmapDb — cannot open: " + dbPath);
        }
        helper::InitSchema(db);
    }

    inline BeatmapDb::~BeatmapDb() {
        if (db) {
            sqlite3_close(db);
        }
    }

    inline std::optional<BeatmapEntry> FindByMd5(BeatmapDb& bdb, const std::string& md5) {
        const char* sql =
            "SELECT file_path, md5, title, artist, creator, version, beatmap_id, beatmap_set_id "
            "FROM beatmaps WHERE md5 = ? LIMIT 1;";
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(bdb.db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return std::nullopt;
        }
        sqlite3_bind_text(stmt, 1, md5.c_str(), -1, SQLITE_STATIC);
        std::optional<BeatmapEntry> result;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            BeatmapEntry e;
            e.file_path = helper::ColText(stmt, 0);
            e.md5 = helper::ColText(stmt, 1);
            e.title = helper::ColText(stmt, 2);
            e.artist = helper::ColText(stmt, 3);
            e.creator = helper::ColText(stmt, 4);
            e.version = helper::ColText(stmt, 5);
            e.beatmap_id = sqlite3_column_int(stmt, 6);
            e.beatmap_set_id = sqlite3_column_int(stmt, 7);
            result = e;
        }
        sqlite3_finalize(stmt);
        return result;
    }

}

