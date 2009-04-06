/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation
with the OpenSSL special exemption.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef _FAKEDOWNLOAD_H
#define _FAKEDOWNLOAD_H

class FakeDownload : public Transfer
{
public:
	virtual void changeActive(bool) {}
	virtual void speeds(int& down, int& up) const { down=m_nDownLimitInt; up=m_nUpLimitInt; }
	virtual qulonglong total() const { return 1024*1024; }
	virtual qulonglong done() const { return 1024*512; }
	virtual QString name() const { return m_strName; }
	virtual QString myClass() const { return "FakeDownload"; }
	
	virtual void init(QString uri,QString)
	{
		m_strName = uri;
	}
	
	virtual void load(const QDomNode& map)
	{
		Transfer::load(map);
		m_strName = getXMLProperty(map, "name");
	}
	
	virtual void save(QDomDocument& doc, QDomNode& map) const
	{
		Transfer::save(doc, map);
		setXMLProperty(doc, map, "name", m_strName);
	}
	
	void setSpeedLimits(int,int) {}
	virtual QString uri() const { return m_strName; }
	virtual QString object() const { return QString(); }
	virtual void setObject(QString) { }
	
	static Transfer* createInstance() { return new FakeDownload; }
	static int acceptable(QString, bool) { return 1; }
private:
	QString m_strName;
};

#endif
