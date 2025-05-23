//
// Created by talik on 3/2/2024.
//

#include <QGridLayout>
#include <QFile>
#include <QPlainTextEdit>
#include <QPainter>
#include <QTextBlock>
#include <QTextEdit>
#include <QString>
#include <QMouseEvent>
#include <QTextStream>  // For text files
#include <QProcess>

#include "Editor.h"
#include "LineNumberArea.h"
#include "AppUi.h"
#include "../utils/Utils.h"

#define string_equals(keyText, key) \
(std::equal(keyText.begin(), keyText.end(), key));

// Capture keywords in a given line
static QRegularExpression keywordsRegex(
		QStringLiteral(
				"^\\s*\\b(echo|ls|ps|Write-Output|Get-ChildItem|Connect-SPOService|"
				"Get-SPOsite|Set-SPOUser|Install-Module)\\b(.*)$"),
		QRegularExpression::CaseInsensitiveOption);

// Capture comments at the start of in a given line
static QRegularExpression commentsRegex(QStringLiteral("^\\s*(#\\w*)"));

// Capture string literals in a given line
static QRegularExpression doubleQuotesLiteralRegex(QStringLiteral("\"(.*?)\""));

// Capture variables in a given line
static QRegularExpression variablesRegex(
		QStringLiteral("^\\s*(\\$\\w+\\s*)=(.*?)$"),
		QRegularExpression::CaseInsensitiveOption);

Editor::Editor(QWidget *parent) : QPlainTextEdit(parent), parent(parent) {
	lineNumberArea = new LineNumberArea(this);

	// setting up the editor's layout.
	auto *layoutC = new QGridLayout;
	layoutC->setSpacing(0);
	layoutC->setContentsMargins(0, 0, 0, 0);

	setLayout(layoutC);

	{ // update place holder text
		QFile file(":/test/temp.ps1");
		file.open(QFile::OpenModeFlag::ReadOnly);

		QString fileContent = QString::fromLatin1(file.readAll());
		setPlaceholderText(fileContent);

		file.close();
	}

	connect(this, &Editor::cursorPositionChanged, this, &Editor::highlightCurrentLine);
	connect(this, &Editor::updateRequest, this, &Editor::updateLineNumberArea);

	// Enables syntaxHighlighting i.e. showing keywords, comments, variables etc.
	connect(this, &Editor::syntaxtHighlightingEvent, this, &Editor::documentSyntaxHighlighting);
	connect(this, &Editor::inlineSyntaxtHighlightingEvent, this, &Editor::inlineSyntaxHighlighting);

	// Enables auto saving the document
	connect(this, &Editor::readyToSaveEvent, this, &Editor::autoSave);

	setStyleSheet("border: 0; color: #c0c0c0; margin-left: 60px; Editor.test {};");
	updateLineNumberAreaWidth(0);

	play = QIcon(QIcon(ItoolsNS::main_config.getAppIcons().playCode));
	play.actualSize(QSize(64, 64));
}

void Editor::updateLineNumberArea(const QRect &rect, int dy) {
	if (dy)
		lineNumberArea->scroll(0, dy);
	else
		lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());

	if (rect.contains(viewport()->rect()))
		updateLineNumberAreaWidth(0);
}

void Editor::lineNumberAreaPaintEvent(QPaintEvent *event) {
	QPainter painter(lineNumberArea);
	painter.setBackgroundMode(Qt::BGMode::TransparentMode);

	QTextBlock block = firstVisibleBlock();
	int blockNumber = block.blockNumber();
	int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
	int bottom = top + qRound(blockBoundingRect(block).height());

	QString editorText = toPlainText();
	// don't set line numbers until the placeholder text is cleared.
	while (block.isValid() && top <= event->rect().bottom() && !editorText.isEmpty()) {
		if (block.isVisible() && bottom >= event->rect().top()) {

			int i = blockNumber + 1;
			QString number = QString::number(i);

			// Set background color
			QRect rect(0, top, lineNumberArea->width(), fontMetrics().height());
			QColor lineColor = QColor(Qt::lightGray).lighter(25);

			// Highlight the line number that is active
			QTextCursor cursor = textCursor();
			if (cursor.blockNumber() == block.blockNumber()) {
				painter.fillRect(rect, lineColor);  // Fill with light gray
			}

			// Adds a margin-left to the line rectangle
			rect.setLeft(4);

			painter.drawText(rect, Qt::AlignLeft, number);
		}

		block = block.next();
		top = bottom;
		bottom = top + qRound(blockBoundingRect(block).height());
		++blockNumber;
	}
}

int Editor::lineNumberAreaWidth() {
	int digits = 1;
	int max = qMax(1, blockCount());
	while (max >= 10) {
		max /= 10;
		++digits;
	}

	int space = 64 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;

	return space;
}

void Editor::updateLineNumberAreaWidth(int /* newBlockCount */) {
	setViewportMargins(0, 0, 0, 0);
}

void Editor::highlightCurrentLine() {
	QList<QTextEdit::ExtraSelection> extraSelections;

	if (!isReadOnly()) {
		QTextEdit::ExtraSelection selection;

		QColor lineColor = QColor(Qt::lightGray).lighter(25);

		selection.format.setBackground(lineColor);
		selection.format.setProperty(QTextFormat::FullWidthSelection, true);
		selection.cursor = textCursor();

		extraSelections.append(selection);
	}

	setExtraSelections(extraSelections);
}

QString Editor::convertRhsTextToHtml(const QString &qRhsLine) {
	//qDebug() << qRhsLine;

	QRegularExpressionMatch doubleQuotesLiteralExp = doubleQuotesLiteralRegex.match(qRhsLine);
	if (doubleQuotesLiteralExp.hasMatch()) {

		//@formatter:off
		return QString("<span style='color:#3eb489'>" +
					   qRhsLine +
					   "</span>");
		//@formatter:on
	}

	return qRhsLine;
}

QString Editor::convertTextToHtml(QString &line) {
	QString lineBreak("<p> </p>");
//	QString lineBreak("<br/>");
//	QString lineBreak("\r");

	if (line.isEmpty()) {
		// FIXME what is the best way to add new lines? <br/> adds extra lines
		return lineBreak;
	}

	// Handles comments on if # is found at the beginning of the line
	QRegularExpressionMatch commentAtStartOfLineExp = commentsRegex.match(line);
	if (commentAtStartOfLineExp.hasMatch()) {
		return "<p style='color:gray'>" + line + "</p>";
	}

	QRegularExpressionMatch keywordsExp = keywordsRegex.match(line);
	if (keywordsExp.hasMatch()) {
		auto keyword = keywordsExp.captured(1);
		auto rhsTextToHtml = convertRhsTextToHtml(keywordsExp.captured(2));

		//qDebug() << rhsTextToHtml;
		//qDebug() << keywordsExp.capturedTexts();

		//@formatter:off
		return QString("<p>"
				"<span style='color:#FFB76B'>" + keyword + "</span>" +
				rhsTextToHtml +
				"</p>");
		//@formatter:on
	}

	QRegularExpressionMatch variablesRegexExp = variablesRegex.match(line);
	if (variablesRegexExp.hasMatch()) {
		auto variable = variablesRegexExp.captured(1);
		auto rhsTextToHtml = convertRhsTextToHtml(variablesRegexExp.captured(2));

		//@formatter:off
		return QString("<p>"
					   "<span style='color:#87CEEB'>" + variable + "</span>" +
					   "=" +
					   rhsTextToHtml +
					   "</p>");
		//@formatter:on
	}

	if (std::equal(line.begin(), line.end(), " ")) return lineBreak;

	//@formatter:off
	return QString("<p style='text-decoration: green wavy underline;'>"
					+ line + // TODO underline only the first word
				   "</p>");
	//@formatter:on
}

void Editor::openAndParseFile(const QString &filePath, QFile::OpenModeFlag modeFlag) {
	if (modeFlag != QFile::ReadOnly) {
		this->currentFile = filePath;
	}

	QFile file(filePath);
	file.open(modeFlag);

	QString fileContent = QString::fromLatin1(file.readAll());
	file.close(); // close file

	// clear editor before using the function to avoid adding to the previous opened file
	setPlainText(fileContent);

	// highlight syntax
	emit syntaxtHighlightingEvent();
}

void Editor::autoSave() {
	auto appUi = dynamic_cast<AppUi *>(parent);

	// auto save works only if the file had been saved before
	// therefore currentFile should have been set
	if (appUi != nullptr && !currentFile.isEmpty()) {
		auto statusBar = appUi->getQStatusBar();

		// Open the file for writing
		QFile file(currentFile);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			statusBar->showMessage(file.errorString());
		}

		// Write data to the file (using QTextStream for text files)
		QTextStream out(&file);
		out << toPlainText();

		statusBar->showMessage("Auto Saving..", 2000);

		// Close the file (important to flush data to disk)
		file.close();
	}
}

void Editor::focusInEvent(QFocusEvent *e) {
	// TODO -- perhaps can use this event to update and show the line number soon as focus is active.
	QPlainTextEdit::focusInEvent(e);
}

void Editor::keyReleaseEvent(QKeyEvent *e) {
	// default behaviour
	QPlainTextEdit::keyReleaseEvent(e);

	QString keyText = e->text();
	// event to signal Auto Save can occur;
	bool isBackSpaceKey = string_equals(keyText, "\b");
	bool isEnterKey = string_equals(keyText, "\r");
	bool isUndo = string_equals(keyText, "\u001A");

	if (keyText.isEmpty()) { return; }

	if (isUndo) {
		// revert to previous version of the document
		return;
	}

	if (!isBackSpaceKey and !isEnterKey) {
		emit inlineSyntaxtHighlightingEvent();
		return;
	}

	QString currentText = toPlainText();
	// Analyze the difference between previousText and currentText to identify deletions
	auto deletedLength = previousText.size() - currentText.size();

	if (isBackSpaceKey && deletedLength > 0) {
		emit inlineSyntaxtHighlightingEvent();
	} else {
		emit readyToSaveEvent();
	}
}

void Editor::mousePressEvent(QMouseEvent *e) {
	// clear placeholder text since the user is active in the area
	setPlaceholderText("");

	QPlainTextEdit::mousePressEvent(e);
}

void Editor::inlineSyntaxHighlighting() {
	QTextCursor cursor = textCursor();
	const int position = cursor.position();

	// Create a new text cursor positioned at the beginning of the block
	QTextBlock blockToReplace = cursor.block();
	QTextCursor nextCursor(blockToReplace);
	nextCursor.select(QTextCursor::SelectionType::LineUnderCursor);

	QString text = blockToReplace.text();
	QString html = convertTextToHtml(text);

	nextCursor.insertHtml(
			"<pre>" +
			html +
			"</pre>"
	);
	nextCursor.setPosition(position);
	setTextCursor(nextCursor);

	//save after successful syntax highlighting
	emit readyToSaveEvent();
}

void Editor::documentSyntaxHighlighting() {

	QString plainText = toPlainText();
	if (!plainText.isEmpty()) {
		// creating opening tags for HTML string
		QString html("<pre>");
		for (auto line: plainText.split("\n")) {
			// Process the line here
			html.append(convertTextToHtml(line));
		}
		// closing tags for HTML string
		html.append("</pre>");

		// update the UI with the new formatted code.
		clear();
		appendHtml(html);
	}

	// save after successful syntax highlighting
	emit readyToSaveEvent();
}

void Editor::keyPressEvent(QKeyEvent *e) {
	previousText = toPlainText();

	QPlainTextEdit::keyPressEvent(e);
}
