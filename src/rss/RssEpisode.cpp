/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2008 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 3 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "RssFetcher.h"
#include <QDate>
#include <QRegularExpression>
#include <algorithm>

QString RssFetcher::generateEpisodeName(const RssRegexp& match, QString itemName)
{
	QString rval;
	QChar zero('0');
	
	if(match.tvs == RssRegexp::None)
		return QString();
	else if(match.tvs == RssRegexp::SeasonBased)
	{
		QRegularExpression matcher1("(\\d+)x(\\d+)"), matcher2("S(\\d+)E(\\d+)");
		QRegularExpressionMatch m1 = matcher1.match(itemName), m2 = matcher2.match(itemName);
		if(m1.hasMatch())
			rval = QString("S%1E%2").arg(m1.captured(1).toInt(),2,10,zero).arg(m1.captured(2).toInt(),2,10,zero);
		else if(m2.hasMatch())
			rval = QString("S%1E%2").arg(m2.captured(1).toInt(),2,10,zero).arg(m2.captured(2).toInt(),2,10,zero);
	}
	else if(match.tvs == RssRegexp::EpisodeBased)
	{
		QRegularExpression matcher("\\d+");
		QRegularExpressionMatch m = matcher.match(itemName);
		if(m.hasMatch())
			rval = QString("%1").arg( m.captured(0).toInt(), 4, 10, zero );
	}
	else if(match.tvs == RssRegexp::DateBased)
	{
		QRegularExpression matcher1("(\\d{4})[\\-\\. ](\\d\\d)[\\-\\. ](\\d\\d)"), matcher2("(\\d\\d)[\\-\\. ](\\d\\d)[\\-\\. ](\\d{2,4})"),
				 matcher3("(\\d\\d?)[\\-\\. ](\\w{3,})[\\-\\. ](\\d{2,4})");
		QRegularExpressionMatch m1 = matcher1.match(itemName), m2 = matcher2.match(itemName), m3 = matcher3.match(itemName);
		if(m1.hasMatch())
		{
			int month = m1.captured(2).toInt(), day = m1.captured(3).toInt();
			dayMonthHeuristics(day, month);
			rval = QString("%1-%2-%3").arg(m1.captured(1).toInt()).arg(month,2,10,zero).arg(day,2,10,zero);
		}
		else if(m2.hasMatch())
		{
			int year = m2.captured(3).toInt();
			if(year < 100)
				year += 2000;
			int day = m2.captured(1).toInt();
			int month = m2.captured(2).toInt();
			
			dayMonthHeuristics(day, month);
			rval = QString("%1-%2-%3").arg(year).arg(month,2,10,zero).arg(day,2,10,zero);
		}
		else if(m3.hasMatch())
		{
			const char* months[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
			int month = 0;
			int year = m3.captured(3).toInt();
			if(year < 100)
				year += 2000;
			
			QString m = m3.captured(2);
			for(int i=0;i<12;i++)
			{
				QString sshort = months[i];
				sshort.resize(3);
				
				if(!m.compare(months[i], Qt::CaseInsensitive) || !m.compare(sshort, Qt::CaseInsensitive))
				{
					month = i+1;
					break;
				}
			}
			
			if(month)
			{
				rval = QString("%1-%2-%3").arg(year).arg(month,2,10,zero).arg(m3.captured(1).toInt(),2,10,zero);
			}
		}
	}
	
	if(!rval.isEmpty())
	{
		if(itemName.contains("trailer", Qt::CaseInsensitive)
			|| itemName.contains("teaser", Qt::CaseInsensitive))
		{
			if(match.includeTrailers)
				rval += "|trailer";
			else
				rval.clear();
		}
		if(itemName.contains("repack", Qt::CaseInsensitive))
		{
			if(match.includeRepacks)
				rval += "|repack";
		}
		if(itemName.contains("proper", Qt::CaseInsensitive))
		{
			if(match.includeRepacks)
				rval += "|proper";
		}
	}
	
	return rval;
}

void RssFetcher::dayMonthHeuristics(int& day, int& month)
{
	// Some Americans are complete idiots when it comes to writing dates
	// Hence we have to do some guessing to recognize which field is the month and which one is the day
	QDate date = QDate::currentDate();
			
	if(month > 12)
		std::swap(month, day);
	else
	{
		int prevmonth = date.month() - 1;
		if(!prevmonth)
			prevmonth = 12;
				
		if(month != date.month() && month != prevmonth && (day == date.month() || day == prevmonth))
			std::swap(month, day);
	}
}

bool RssFetcher::isValidEpisodeId(RssRegexp::TVSType tvs, const QString& id)
{
	QString pattern;
	
	switch(tvs)
	{
	case RssRegexp::SeasonBased:
		pattern = "S\\d{2,}E\\d{2,}"; break;
	case RssRegexp::EpisodeBased:
		pattern = "\\d{4,}"; break;
	case RssRegexp::DateBased:
		pattern = "\\d{4}-\\d{2}-\\d{2}"; break;
	default:
		return false;
	}
	
	pattern += "(\\|(trailer|repack|proper))*";
	return QRegularExpression("^(" + pattern + ")$").match(id).hasMatch();
}
