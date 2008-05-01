#ifndef VIDEOFETCHER_H
#define VIDEOFETCHER_H
#include <QWidget>
#include "ui_VideoFetcher.h"

class QBuffer;

class VideoFetcher : public QWidget, Ui_VideoFetcher
{
Q_OBJECT
public:
	VideoFetcher();
	static QWidget* create() { return new VideoFetcher; }
	
	QString decodeYouTube(QByteArray data, QString url);
	QString decodeStreamCZ(QByteArray data, QString url);
public slots:
	void openLink(const QString& link);
	void decode();
	void download();
	void downloadDone(bool error);
private:
	void doNext();
	void finalize();
	
	QStringList m_videos;
	QStringList m_unsupp, m_err;
	QString m_now;
	int m_nextType;
	QBuffer* m_buffer;

public:
	struct Fetcher
	{
		const char* name;
		const char* re;
		QString (VideoFetcher::*lpfn)(QByteArray data, QString url);
	};
};

#endif
