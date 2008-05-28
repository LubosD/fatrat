#include "DataPoller.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

const int MAX_SPEED_SAMPLES = 5;
const int SOCKET_TIMEOUT = 15;

DataPoller* DataPoller::m_instance = 0;

DataPoller::DataPoller() : m_bAbort(false)
{
	m_instance = this;
	m_epoll = epoll_create(10);
	
	start();
}

DataPoller::~DataPoller()
{
	m_bAbort = true;
	wait();
	close(m_epoll);
}

void DataPoller::run()
{
	timeval lastT, nowT;
	
	gettimeofday(&lastT, 0);
	
	while(!m_bAbort)
	{
		epoll_event events[20];
		int num = epoll_wait(m_epoll, events, sizeof(events) / sizeof(events[0]), 500);
		
		for(int i=0;i<num;i++)
		{
			int errn = 0;
			TransferInfo& info = m_sockets[events[num].data.fd];
			
			if(events[i].events & EPOLLERR)
				errn = ERR_CONNECTION_ERR;
			else if(events[i].events & EPOLLHUP)
				errn = ERR_CONNECTION_LOST;
			else if(events[i].events & EPOLLIN && !info.bUpload)
				errn = processRead(info);
			else if(events[i].events & EPOLLOUT && info.bUpload)
				errn = processWrite(info);
			
			if(errn != 0 || !info.toTransfer)
			{
				emit processingDone(info.socket, errn);
				removeSocket(info.socket);
			}
		}
		
		gettimeofday(&nowT, 0);
		
		qint64 usecs = qint64(nowT.tv_sec-lastT.tv_sec)*1000000 + nowT.tv_usec-lastT.tv_usec;
		if(usecs > 1000000)
		{
			for(QMap<int, TransferInfo>::iterator it = m_sockets.begin(); it != m_sockets.end();)
			{
				qint64 bytes = it->transferred - it->transferredPrev;
				it->speedData << QPair<int,qint64>(usecs/1000, bytes);
				
				if(it->speedData.size() > MAX_SPEED_SAMPLES)
					it->speedData.removeFirst();
				
				qint64 tTime = 0;
				bytes = 0; // reusing the variable
				for(int j=0;j<it->speedData.size();j++)
				{
					tTime += it->speedData[j].first;
					bytes += it->speedData[j].second;
				}
				
				it->speed = bytes/tTime*1000;
				it->transferredPrev = it->transferred;
				
				if(nowT.tv_sec - it->lastProcess.tv_sec > SOCKET_TIMEOUT)
				{
					emit processingDone(it->socket, ERR_CONNECTION_TIMEOUT);
					epoll_ctl(m_epoll, EPOLL_CTL_DEL, it->socket, 0);
					it = m_sockets.erase(it);
					continue;
				}
				it++;
			}
			
			lastT = nowT;
		}
	}
}

qint64 DataPoller::getSpeed(int sock)
{
	return m_sockets[sock].speed;
}

qint64 DataPoller::getProgress(int sock)
{
	return m_sockets[sock].transferred;
}

void DataPoller::setSpeedLimit(int sock, qint64 limit)
{
	m_sockets[sock].speedLimit = limit;
}

void DataPoller::addSocket(int socket, int file, bool bUpload, qint64 toTransfer)
{
	TransferInfo& info = m_sockets[socket];
	epoll_event ev;
	
	ev.data.fd = socket;
	
	int arg = fcntl(socket, F_GETFL);
	fcntl(socket, F_SETFL, arg | O_NONBLOCK);
	
	ev.events = EPOLLHUP | EPOLLERR;
	ev.events |= (bUpload) ? EPOLLOUT : EPOLLIN;
	
	epoll_ctl(m_epoll, EPOLL_CTL_ADD, socket, &ev);
	
	info.bUpload = bUpload;
	info.toTransfer = toTransfer;
	info.transferred = info.transferredPrev = 0;
	
	gettimeofday(&info.lastProcess, 0);
}

void DataPoller::removeSocket(int sock)
{
	m_sockets.remove(sock);
	epoll_ctl(m_epoll, EPOLL_CTL_DEL, sock, 0);
}

int DataPoller::processRead(TransferInfo& info)
{
	qint64 thisCall = 0, thisLimit = 0;
	char buffer[4096];
	timeval tv;
	
	gettimeofday(&tv, 0);
	
	if(info.speedLimit)
	{
		thisLimit = (tv.tv_sec-info.lastProcess.tv_sec)*info.speedLimit;
		thisLimit += qint64(tv.tv_usec-info.lastProcess.tv_usec)*double(info.speedLimit)/1000000;
	}
	
	while((thisCall < thisLimit || !thisLimit) && (info.toTransfer > 0 || info.toTransfer < 0))
	{
		ssize_t r = read(info.socket, buffer, qMin<ssize_t>(sizeof buffer, thisLimit - thisCall));
		
		if(r < 0)
		{
			if(errno == EAGAIN)
				break;
			else
				return errno;
		}
		else if(r == 0)
			return -1;
		
		if(write(info.file, buffer, r) != r)
			return errno;
		
		if(info.toTransfer > 0)
			info.toTransfer -= r;
		info.transferred += r;
		thisCall += r;
	}
	
	gettimeofday(&info.lastProcess, 0);
	return 0;
}

int DataPoller::processWrite(TransferInfo& info)
{
}
