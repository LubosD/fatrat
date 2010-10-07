/*
FatRat download manager
http://fatrat.dolezel.info

Copyright (C) 2006-2010 Lubos Dolezel <lubos a dolezel.info>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

In addition, as a special exemption, Luboš Doležel gives permission
to link the code of FatRat with the OpenSSL project's
"OpenSSL" library (or with modified versions of it that use the; same
license as the "OpenSSL" library), and distribute the linked
executables. You must obey the GNU General Public License in all
respects for all of the code used other than "OpenSSL".
*/

#include "JByteBuffer.h"
#include "JVM.h"

JByteBuffer::JByteBuffer(jobject obj)
{
	JNIEnv* env = *JVM::instance();
	m_object = env->NewGlobalRef(obj);
}

JByteBuffer::JByteBuffer(void* mem, size_t len)
{
	JNIEnv* env = *JVM::instance();
	jobject obj = env->NewDirectByteBuffer(mem, len);
	m_object = env->NewGlobalRef(obj);
	env->DeleteLocalRef(obj);
}

JByteBuffer::JByteBuffer(const JByteBuffer& b)
{
	if (b.m_object)
	{
		JNIEnv* env = *JVM::instance();
		m_object = env->NewGlobalRef(b.m_object);
	}
	m_buffer = b.m_buffer;
}

JByteBuffer::JByteBuffer(size_t len)
{
	JNIEnv* env = *JVM::instance();

	m_buffer.reset(new char[len]);

	jobject obj = env->NewDirectByteBuffer(m_buffer.get(), len);
	m_object = env->NewGlobalRef(obj);
	env->DeleteLocalRef(obj);
}

void* JByteBuffer::allocate(size_t len)
{
	JNIEnv* env = *JVM::instance();
	if (m_object)
		env->DeleteGlobalRef(m_object);

	m_buffer.reset(new char[len]);

	jobject obj = env->NewDirectByteBuffer(m_buffer.get(), len);
	m_object = env->NewGlobalRef(obj);
	env->DeleteLocalRef(obj);

	return m_buffer.get();
}

void* JByteBuffer::getBuffer()
{
	if (!m_object)
		return 0;
	JNIEnv* env = *JVM::instance();
	return env->GetDirectBufferAddress(m_object);
}

size_t JByteBuffer::getLength()
{
	if (!m_object)
		return 0;
	JNIEnv* env = *JVM::instance();
	return env->GetDirectBufferCapacity(m_object);
}

JByteBuffer& JByteBuffer::operator=(JByteBuffer& buf)
{
	JNIEnv* env = *JVM::instance();
	if (m_object)
	{
		env->DeleteGlobalRef(m_object);
		m_object = 0;
	}
	if (buf.m_object)
		m_object = env->NewGlobalRef(buf.m_object);
	m_buffer = buf.m_buffer;

	return *this;
}

JByteBuffer& JByteBuffer::operator=(jobject obj)
{
	JNIEnv* env = *JVM::instance();
	if (m_object)
		env->DeleteGlobalRef(m_object);
	m_object = env->NewGlobalRef(m_object);
	m_buffer.reset();

	return *this;
}
