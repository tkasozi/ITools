// MIT License
//
// Copyright (c)  "2025" Talik A. Kasozi
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//
// Created by talik on 4/26/2024.
//

#ifndef IT_TOOLS_DB_CONN_H
#define IT_TOOLS_DB_CONN_H

#include <QSqlDatabase>
#include <QMessageBox>
#include <QSqlError>
#include <QDate>
#include <QSqlQuery>
#include "FileObject.h"
#include <fstream>

const auto FILES_SQL = QLatin1String(R"(
    CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, file_path VARCHAR UNIQUE, file_name VARCHAR);
)");

const auto INSERT_FILE_SQL = QLatin1String(R"(
    INSERT INTO files(file_path, file_name) VALUES(?, ?);
)");

const auto SELECT_FILES_SQL = QLatin1String(R"(
    SELECT * FROM files;
)");

const auto SELECT_FILE_BY_FILE_PATH_SQL = QLatin1String(R"(SELECT * FROM files WHERE file_path = ?;)");

static QVariant insertFile(const QString &filePath, const QString &title) {
	QSqlQuery query;
	if (!query.prepare(INSERT_FILE_SQL)) {
		// "Error executing query:" << query.lastError().text();
	}

	query.addBindValue(filePath);
	query.addBindValue(title);
	query.exec();

	return query.lastInsertId();
}

static QList<FileObject *> findPreviouslyOpenedFiles() {
	QSqlQuery query;

	if (!query.exec(SELECT_FILES_SQL)) {
		// qDebug() << "Error executing query:" << query.lastError().text();
	}

	QList<FileObject *> files;
	while (query.next()) {
		auto file = new FileObject;

		int id = query.value(0).toInt();
		QString filePath = query.value(1).toString(); // column file_path
		QString title = query.value(2).toString(); // column title

		file->setFilePath(filePath);
		file->setFileName(title);
		files.append(file);
	}

	return files;
}

static bool db_conn() {
	const char * dbName = "0a97fd39-aac6-463c-9b91-a3f8a7649ef0.db";

	std::fstream db(dbName, std::ios::in);

	if (db.is_open()) {
		// db file exists
		db.close();
	} else {
		std::ofstream outputFile(dbName, std::ios::out);
	}

	try {
		QSqlDatabase dbEngine = QSqlDatabase::addDatabase("QSQLITE");
		dbEngine.setDatabaseName(dbName);

		if (!dbEngine.open()) {
			QMessageBox::critical(nullptr, QObject::tr("Cannot open database"),
								  "Unable to establish a database connection.\n"
								  "Click Cancel to exit.", QMessageBox::Cancel);
			return false;
		}
	} catch (_exception exception) {
		return false;
	}

	return true;
}

static QSqlError init_db() {

	QSqlQuery query;
	if (!query.exec(FILES_SQL)) {
		// qDebug() << "Error executing query:" << query.lastError().text();
		return query.lastError();
	}

	return {};
}

#endif //IT_TOOLS_DB_CONN_H
